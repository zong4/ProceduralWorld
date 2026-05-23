# Procedural World 项目评分说明与答辩指南

本文档用于向老师说明本项目完成了哪些技术工作，以及答辩时如何对照代码解释实现。建议答辩时不要只说“做了某功能”，而是按“数据怎么生成、怎么传到 GPU、shader 怎么使用、界面怎么验证”的顺序讲。

## 1. 项目一句话概述

本项目是一个基于 C++17 + OpenGL 4.1 的实时程序化星球渲染系统。核心内容包括：cube-sphere 星球网格、CPU 四叉树 LOD、GPU tessellation、程序化地形/气候/生物群系生成、水文与侵蚀模拟、基于侵蚀流量 mask 的分形支流河网渲染、FFT 海洋、平面反射/折射、深度水色混合、Fresnel 反射和简化大气散射。

运行入口和主要模块：

| 模块 | 主要代码 |
| --- | --- |
| 程序入口、窗口、UI、输入 | `src/main.cpp` |
| 星球渲染、LOD、FBO、绘制流程 | `src/PlanetRenderer.cpp`, `include/PlanetRenderer.h` |
| 程序化高度/气候/生物群系/侵蚀数据 | `src/PlanetProceduralData.cpp`, `include/PlanetProceduralData.h` |
| FFT 海洋频谱和贴图上传 | `src/FFTOcean.cpp`, `include/FFTOcean.h` |
| 高度场贴图封装 | `src/PlanetHeightField.cpp`, `include/PlanetHeightField.h` |
| 地形 shader | `shaders/terrain.vert`, `shaders/terrain.tesc`, `shaders/terrain.tese`, `shaders/terrain.frag`, `shaders/terrain_surface.glsl`, `shaders/planet_sampling.glsl` |
| 海洋 shader | `shaders/ocean.vert`, `shaders/ocean.tesc`, `shaders/ocean.tese`, `shaders/ocean.frag` |
| 大气 shader | `shaders/atmosphere.vert`, `shaders/atmosphere.frag` |

构建方式：

```powershell
xmake build
xmake run
```

## 2. 推荐答辩讲解顺序

建议按实际执行顺序讲：

1. `main.cpp` 初始化窗口、OpenGL、ImGui 和 `PlanetRenderer`。
2. `PlanetRenderer` 创建 shader、网格、FBO、海洋和大气资源。
3. `PlanetProceduralData` 在 CPU 侧生成六个 cube face 的高度、温度、湿度、生物群系和侵蚀数据。
4. `PlanetRenderer` 把 CPU 数据打包成 GPU texture array，包括高度图、生物群系图、侵蚀 mask 等。
5. 每帧根据相机位置更新四叉树 LOD，剔除不可见 patch，并提交地形 patch。
6. 地形 shader 采样高度图，把 cube face 上的点归一化到球面，再按高度偏移得到星球表面。
7. 侵蚀后处理会从高地源头沿地形/flow 方向提取主干河道和多条细支流，地形 fragment shader 再根据 channel/flow mask 叠加河水、湿润岸线、水面高光和折射式扰动。
8. 海洋模块更新 FFT 波浪，把频谱结果写入高度、法线、位移贴图。
9. 渲染海洋前先渲染反射 FBO 和折射 FBO，海洋 shader 再采样这两张贴图做 Fresnel 混合。
10. 最后绘制大气壳，用 Rayleigh/Mie 散射近似天空和大气边缘。
11. ImGui 面板提供参数调节、debug mask、性能统计和会话保存。

## 3. 功能点一：cube-sphere 星球几何

### 做了什么

项目不是渲染一个普通平面地形，而是把星球拆成六个 cube face，每个 face 再细分为网格 patch。shader 中把 cube face 坐标转换成立方体上的方向向量，再归一化成球面方向，最后乘以半径和高度偏移得到球面地形。

### 代码位置

| 代码 | 说明 |
| --- | --- |
| `PlanetRenderer::kPlanetFaces` | 定义六个 cube face 的朝向、切线和副切线 |
| `TerrainMesh::buildGrid` | 生成一个规则 patch 网格 |
| `shaders/planet_sampling.glsl` | cube face 坐标、球面方向和贴图采样工具函数 |
| `shaders/terrain.vert` | 传递 patch 局部坐标 |
| `shaders/terrain.tese` | tessellation 后计算球面位置 |

### 答辩时怎么说

可以这样回答：

> 我用 cube-sphere 表示星球。CPU 侧维护六个 cube face，每个 face 上是二维 patch 坐标。shader 根据 face 的 center、uAxis、vAxis 把二维坐标变成立方体上的三维点，然后 normalize 得到球面方向，再用 `planetRadius + height * heightScale` 得到最终顶点位置。这样比经纬度球面更容易做四叉树 LOD，也避免了经纬度网格在极点的严重压缩。

如果老师问“高度怎么贴到球上”：

> 高度不是沿世界 Y 轴抬升，而是沿当前球面法线方向抬升。每个采样点先得到 sphere direction，再乘以半径加高度偏移。

## 4. 功能点二：CPU 四叉树 LOD + GPU tessellation

### 做了什么

地形细节由两层机制控制：

1. CPU 四叉树决定一个 face 上哪些 patch 要继续细分。
2. GPU tessellation shader 决定每个 patch 内部再细分多少三角形。

CPU LOD 根据相机距离、屏幕投影大小、视锥剔除、地平线剔除、海岸线覆盖率等因素选择 patch。GPU tessellation 再根据距离、边缘和侵蚀/生物群系复杂度调整 tessellation level。

### 代码位置

| 代码 | 说明 |
| --- | --- |
| `PlanetRenderer::collectVisiblePatches` | 遍历四叉树并收集当前帧可见 patch |
| `PlanetRenderer::shouldSplitNode` | 判断一个 patch 是否要继续细分 |
| `PlanetRenderer::analyzePatchWaterCoverage` | 分析 patch 是否靠近海岸，影响 LOD |
| `PlanetRenderer::drawTerrainPass` | 把 patch 数据作为 uniform/instance 信息提交 |
| `shaders/terrain.tesc` | 设置 tessellation level |
| `shaders/terrain.tese` | tessellation evaluation 阶段生成最终顶点 |

### 答辩时怎么说

可以这样回答：

> CPU 负责大尺度 LOD，防止远处 patch 数量过多；GPU tessellation 负责 patch 内部细分，保证近处细节平滑。CPU 四叉树主要根据相机距离和屏幕投影尺寸判断是否拆分，同时做视锥和地平线剔除。靠近海岸的 patch 会被额外提高 LOD，因为水陆交界最容易露出几何和材质问题。

如果老师问“裂缝怎么处理”：

> shader 使用 `fractional_even_spacing` 做 tessellation，能减少不同 tessellation level 之间的不连续。项目里还对 patch 边界和海岸做了保守 LOD，但这不是完整的邻接边 stitching 系统，所以我会说这是基础裂缝缓解，不会声称实现了完整无缝 stitching。

## 5. 功能点三：程序化地形生成

### 做了什么

地形高度在 CPU 侧生成。每个 cube face 保存一张二维高度图，整体对应球面上的六个方向区域。高度由多层噪声组合而成，包括大陆、山脉、盆地、海底等形态。

### 代码位置

| 代码 | 说明 |
| --- | --- |
| `PlanetProceduralData::generate` | 地形数据生成总入口 |
| `PlanetProceduralData::terrainHeight` | 根据球面方向计算基础高度 |
| `PlanetProceduralData::gradientNoise` | 基础梯度噪声 |
| `PlanetProceduralData::fbm` | 分形布朗运动噪声，多 octave 叠加 |
| `PlanetProceduralData::computeWaterClimateFields` | 生成水域、温度、湿度等派生数据 |

### fBM 是什么 noise

fBM 全称是 fractional Brownian motion，中文常说分形布朗运动噪声。它不是一个全新的随机函数，而是把同一种基础噪声按不同频率和振幅叠加：

```text
value = noise(p * freq0) * amp0
      + noise(p * freq1) * amp1
      + noise(p * freq2) * amp2
      + ...
```

通常每一层频率乘以 `lacunarity`，振幅乘以 `gain`。低频层决定大陆和大山的大形状，高频层补充局部细节。

### 答辩时怎么说

可以这样回答：

> 我的基础随机源是 gradient noise，fBM 是在它之上做多 octave 叠加。低频噪声控制大陆板块和整体起伏，高频噪声增加山脊、丘陵和海底细节。最终高度不是一次 noise 直接得到，而是多个地貌因子混合得到，比如大陆 mask、山脉 mask、盆地和海底修正。

## 6. 功能点四：温度、湿度、生物群系和地表材质

### 做了什么

温度和湿度不是完全随机生成后直接使用，而是“噪声基础 + 地理修正 + 水域修正”的组合。

温度主要受以下因素影响：

1. 纬度：赤道附近更热，两极更冷。
2. 海拔：海拔越高温度越低。
3. 噪声扰动：让温度边界不机械。

湿度主要受以下因素影响：

1. 基础湿度噪声。
2. 纬度和气候带。
3. 距海洋或水域的远近。
4. 后续水文/侵蚀数据的影响。

生物群系不是简单 `if else` 选一个类型，而是计算多个 biome weight，让不同地貌之间能平滑过渡。shader 最后根据高度、坡度、生物群系权重、侵蚀数据混合地表颜色。

### 代码位置

| 代码 | 说明 |
| --- | --- |
| `PlanetProceduralData::computeWaterClimateFields` | 计算水域、温度、湿度 |
| `PlanetProceduralData::computeBiomeWeights` | 计算各生物群系权重 |
| `PlanetProceduralData::refineTerrainFromBiomeWeights` | 根据生物群系再修正地形 |
| `PlanetRenderer::updateProceduralTextures` | 把 CPU 数据上传为 GPU 贴图 |
| `shaders/terrain_surface.glsl` | 根据 biome、坡度、侵蚀等混合地表颜色 |
| `shaders/terrain.frag` | 最终地形 fragment 输出和 debug mask |

### 答辩时怎么说

可以这样回答：

> 温度不是纯随机的。它以纬度为主，赤道高、两极低，再叠加海拔降温和少量 fBM 噪声。湿度也不是纯随机的，它有基础湿度噪声，但会被海洋距离、纬度气候带和水文数据修正。这样生物群系分布会更接近自然规律，而不是随机色块。

如果老师问“有没有 texture splatting”：

> 项目实现的是基于生物群系权重、坡度、高度和噪声的材质/颜色混合，逻辑上类似 splatting 的多权重混合。但我没有加载多套真实地表纹理图做传统 texture splatting，所以如果严格按“真实纹理 splatting”打分，这一项应当算部分实现。

## 7. 功能点五：水文、液压侵蚀和热侵蚀

### 做了什么

项目实现了 CPU 侧简化液压侵蚀和热侵蚀。液压侵蚀模拟雨水、流向、沉积物携带能力、侵蚀和沉积；热侵蚀模拟过陡坡面向低处滑落，让山坡更自然。

生成的数据不只改变高度，还会产生侵蚀 mask，包括：

| 通道 | 含义 |
| --- | --- |
| channel | 河道/冲刷通道强度 |
| flow | 水流累积强度 |
| wear | 被侵蚀磨损程度 |
| deposition | 沉积程度 |

这些 mask 会被上传到 GPU，shader 用它们改变颜色、粗糙度和地表细节。

### 代码位置

| 代码 | 说明 |
| --- | --- |
| `PlanetProceduralData::applyErosion` | 液压侵蚀和热侵蚀主逻辑 |
| `PlanetProceduralData::computeHydrology` | 水文相关数据计算 |
| `PlanetProceduralData.cpp` 中 erosion 参数 | 雨量、蒸发、容量、沉积、侵蚀等参数 |
| `PlanetRenderer::updateProceduralTextures` | 把侵蚀数据打包到 RGBA 贴图 |
| `PlanetProceduralData::extractPrimaryRiver` | 从侵蚀结果中追踪主干河道，并提取多条更细的支流 |
| `shaders/terrain_surface.glsl` | 使用侵蚀 mask 影响地表材质，并把 channel/flow mask 渲染成带折射感的河网 |
| `shaders/terrain_lighting.glsl` | 给河流区域增加额外镜面高光 |
| `src/main.cpp` Erosion / Rivers 面板 | UI 调节侵蚀参数和河流显示参数 |

### 答辩时怎么说

可以这样回答：

> 侵蚀是在 CPU 高度图上迭代的。每个格点根据高度差把水流分配给更低的邻居，水流根据坡度和速度得到 sediment capacity。如果当前携带沉积物小于容量，就从地形中侵蚀；如果超过容量，就沉积回地面。热侵蚀则检查相邻格点坡度，超过 talus 阈值时把物质从高处搬到低处。最后我把 channel、flow、wear、deposition 打包进 RGBA mask，shader 用这些 mask 表现河道冲刷和沉积区域。

河流渲染可以这样补充：

> 河流不是单独手画的贴图，而是来自侵蚀结果。侵蚀先产生 flow、wear、channel 等 mask，然后 `extractPrimaryRiver` 会先追踪一条长主干河道，再从多个高地候选源头追踪细支流，让支流倾向汇入主干。主干和支流会写回 channelMask/flowMask，并轻微下切河床。渲染时 shader 用更细的 channelMask 生成河水，用 flowMask 做湿润河岸，还叠加细噪声扰动和 caustic 亮度变化，模拟浅水折射感。

需要注意：

> 这不是 GPU compute 侵蚀，也不是严格物理流体仿真，而是适合程序化地形的网格近似侵蚀。

## 8. 功能点六：FFT 海洋

### 做了什么

海洋不是静态透明平面，而是使用 FFT 频谱生成动态波浪。`FFTOcean` 根据风向、风速、波幅等参数生成 Phillips spectrum，然后每帧随时间更新频谱，再通过 IFFT 得到高度、法线和水平位移贴图。

shader 在球面海洋上采样这些贴图，得到动态波浪、法线扰动和 choppy displacement。

### 代码位置

| 代码 | 说明 |
| --- | --- |
| `FFTOcean::initialize` | 创建频谱和 GPU 贴图 |
| `FFTOcean::generateSpectrum` | 生成初始 Phillips 频谱 |
| `FFTOcean::update` | 根据时间更新频谱并执行 IFFT |
| `FFTOcean::uploadTextures` | 上传高度、法线、位移贴图 |
| `PlanetRenderer::drawOceanPass` | 绘制海洋 |
| `shaders/ocean.tese` | 计算球面海洋顶点位置和波浪位移 |
| `shaders/ocean.frag` | 海水颜色、法线、Fresnel、反射折射混合 |

### 答辩时怎么说

可以这样回答：

> 海洋波浪来自频域模拟。初始化时根据 Phillips spectrum 生成不同波数的复数振幅，每帧根据色散关系更新相位，再做 IFFT 得到空间域高度图。渲染时 ocean shader 把高度图、法线图和位移图采样到球面海洋上，形成动态波浪和法线高光。

如果老师问“是不是完整物理海洋”：

> 它是图形学中常见的 FFT ocean 近似，适合实时渲染。没有做真实流体求解，也没有与地形水体做双向物理耦合。

## 9. 功能点七：平面反射、折射、深度水色混合和 Fresnel

### 做了什么

海洋渲染前会先渲染两张离屏贴图：

1. reflection target：用镜像相机渲染反射画面。
2. refraction target：用正常相机渲染水下/水后的折射画面，并保留深度。

海洋 fragment shader 采样这两张贴图，再结合屏幕深度估算水体厚度。浅水偏亮、偏青，深水偏暗、偏蓝。最后用 Fresnel 根据视线角度混合反射和折射：低角度看水面反射更强，俯视时折射/水色更明显。

### 代码位置

| 代码 | 说明 |
| --- | --- |
| `PlanetRenderer::ensureReflectionResources` | 创建 reflection/refraction FBO |
| `PlanetRenderer::drawReflectionRefractionPasses` | 渲染反射和折射 pass |
| `PlanetRenderer::drawOceanPass` | 绑定反射、折射、深度贴图给海洋 shader |
| `shaders/ocean.frag` | 采样反射/折射贴图，做深度混合和 Fresnel |
| `src/main.cpp` Ocean/Performance 面板 | 开关和性能显示 |

### 答辩时怎么说

可以这样回答：

> 我用两个 framebuffer 做 planar reflection 和 refraction。反射 pass 会把相机相对海平面做镜像，再渲染一次地形；折射 pass 用正常相机渲染，并保存深度。海洋 shader 用屏幕坐标采样反射和折射颜色，同时用 refraction depth 和当前水面深度估算 water column depth。最后用 Schlick Fresnel 近似根据视角混合反射和折射。

需要注意：

> 因为星球是球形的，严格物理上反射面应该是局部曲面。项目中使用的是以海平面高度为基准的 planar approximation，也就是平面近似反射。它能满足实时效果，但不是完整球面光线追踪反射。

## 10. 功能点八：程序化天空和大气散射

### 做了什么

项目实现了一个包围星球的大气壳。fragment shader 对视线和大气球壳求交，在视线方向上积分 Rayleigh 和 Mie 散射，得到天空颜色、地平线大气边缘和太阳方向高光。

### 代码位置

| 代码 | 说明 |
| --- | --- |
| `PlanetRenderer::drawAtmospherePass` | 设置大气参数并绘制大气球 |
| `PlanetRenderer::initialize` | 创建 atmosphere shader 和大气网格 |
| `shaders/atmosphere.vert` | 大气顶点变换 |
| `shaders/atmosphere.frag` | Rayleigh/Mie 散射积分 |
| `src/main.cpp` Sky/Atmosphere 面板 | 调节大气高度、密度、散射强度等 |

### 答辩时怎么说

可以这样回答：

> 大气不是一张天空盒贴图，而是一个真实包围星球的大气球壳。shader 中先计算视线和大气层球体的交点，然后沿视线分步积分。Rayleigh 散射主要负责蓝色天空，Mie 散射负责太阳附近的前向散射和雾感。密度随高度指数衰减，所以越靠近地表大气越浓。

需要注意：

> 目前是简化大气散射，没有做完整昼夜循环、云层阴影或体积云。

## 11. 功能点九：调试界面、性能统计和可验证性

### 做了什么

项目提供 ImGui 调试界面，可以实时调整 LOD、地形、水、侵蚀、海洋、大气等参数，并显示 patch 数量、draw call、GPU pass 耗时、反射/折射贴图开销等性能数据。还可以切换 debug mask 查看高度、温度、湿度、生物群系、侵蚀等中间结果。

### 代码位置

| 代码 | 说明 |
| --- | --- |
| `src/main.cpp` | ImGui 控制面板、快捷键、性能窗口 |
| `PlanetRenderer::PerformanceStats` | 渲染统计数据 |
| `PlanetRenderer::setTerrainMaskDebugMode` | 切换地形 debug 显示 |
| `shaders/terrain.frag` | 输出不同 debug mask |
| `saveSession` / `loadSession` | 会话参数保存和加载 |

### 答辩时怎么说

可以这样回答：

> 我把程序化生成过程的中间数据暴露到 debug view 里，不只看最终效果。比如可以分别查看高度、温度、湿度、生物群系和侵蚀 mask。这样能证明这些数据确实参与了渲染，而不是只做了一个表面颜色。

## 12. 评分点对应表

因为不同老师的评分细则可能不同，下面按常见图形学项目评分点整理：

| 评分点 | 本项目状态 | 代码证据 | 答辩建议 |
| --- | --- | --- | --- |
| 程序化地形 | 已实现 | `PlanetProceduralData::terrainHeight`, `fbm` | 重点讲 fBM、多地貌因子、六面高度图 |
| 球形地形 | 已实现 | `planet_sampling.glsl`, `terrain.tese` | 讲 cube-sphere，不是经纬度球 |
| LOD / tessellation | 已实现 | `collectVisiblePatches`, `terrain.tesc` | 讲 CPU 四叉树 + GPU tessellation 两层 |
| 生物群系 | 已实现 | `computeBiomeWeights`, `terrain_surface.glsl` | 讲温度、湿度、坡度、高度共同决定 |
| 地表材质混合 | 部分/已实现颜色混合 | `terrain_surface.glsl` | 可以说做了权重混合，不要说加载了多套真实纹理 |
| 基础海洋 | 已实现 | `drawOceanPass`, `ocean.frag` | 讲球面海洋、水深、透明和颜色 |
| FFT 动态波浪 | 已实现 | `FFTOcean.cpp`, `ocean.tese` | 讲频谱、IFFT、高度/法线/位移贴图 |
| 液压侵蚀 | 已实现简化版 | `applyErosion` | 讲水流、容量、侵蚀、沉积；说明是 CPU 网格近似 |
| 热侵蚀 | 已实现简化版 | `applyErosion` | 讲坡度超过阈值时物质滑移 |
| 河流可视化 | 已实现 | `extractPrimaryRiver`, `terrain_surface.glsl`, `terrain_lighting.glsl` | 讲分形支流河网、channel/flow mask、湿润岸线、高光和折射式扰动 |
| 反射/折射 | 已实现 | `drawReflectionRefractionPasses`, `ocean.frag` | 讲两个 FBO、深度、Fresnel |
| 大气散射 | 已实现简化版 | `atmosphere.frag` | 讲 Rayleigh/Mie、指数密度、视线积分 |
| Debug/性能 | 已实现 | `main.cpp`, `PerformanceStats` | 展示 debug mask 和 performance 面板 |
| 程序化植被 | 未实现 | 无 | 不要声称实现 |
| 完整真实纹理 splatting | 未完整实现 | 无真实多纹理地表采样 | 可说做了 procedural material blending |
| GPU compute 侵蚀 | 未实现 | 无 compute shader | 不要声称实现 |
| 完整物理大气/昼夜循环 | 未实现 | 无完整太阳轨道/云层系统 | 可说做了简化散射 |

## 13. 老师可能追问的问题和建议回答

### Q1：fBM 到底是什么？

回答：

> fBM 是把基础 gradient noise 用多组频率和振幅叠加。低频控制大形状，高频增加细节。代码里 `fbm` 会循环多个 octave，每层提高频率、降低振幅，所以生成结果有分形层次。

### Q2：温度和湿度是不是先随机一份再改？

回答：

> 可以理解为有噪声基础，但不是纯随机后简单修改。温度以纬度为主，再叠加海拔降温和噪声扰动；湿度以基础噪声为底，再结合纬度、水域距离和水文数据修正。所以温湿度既有自然规律，也有局部变化。

### Q3：生物群系怎么决定？

回答：

> 生物群系由温度、湿度、高度、坡度和水域状态共同决定。代码不是只选一个离散类型，而是计算多个 biome weight，然后在 shader 中平滑混合地表颜色和材质特征。

### Q4：侵蚀是不是真的改变了地形？

回答：

> 是的。侵蚀在 CPU 高度图上迭代，会修改高度数据，同时记录 channel、flow、wear、deposition 这些 mask。高度变化会影响最终几何，mask 会影响 shader 材质表现。

### Q4.1：河流是怎么渲染出来的？

回答：

> 河流不是额外手绘的图片，也不是单独的 mesh。侵蚀阶段会产生 flow/wear/channel mask，后处理 `extractPrimaryRiver` 先追踪主干，再追踪多条更细的支流，让支流汇入主干形成分形状河网。渲染时 `terrain_surface.glsl` 用 channelMask 生成细河水，用 flowMask 暗化湿润河岸，并用高频噪声扰动河床颜色来模拟浅水折射；`terrain_lighting.glsl` 再给河道区域加 specular。

### Q5：FFT 海洋和普通 noise 水面有什么区别？

回答：

> 普通 noise 水面通常只是局部扰动，没有真实波浪频谱。FFT ocean 从频域生成波浪，把不同波长和方向的波叠加，再通过 IFFT 得到空间高度场，更适合表现大面积海面。

### Q6：反射折射怎么做？

回答：

> 先用两个 FBO 离屏渲染。反射 FBO 用镜像相机渲染地形，折射 FBO 用正常相机渲染并保存深度。海洋 shader 采样这两张颜色图，再根据深度估算水体厚度，最后用 Fresnel 按视角混合。

### Q7：为什么说是 planar reflection？

回答：

> 因为反射相机是围绕一个近似海平面做镜像。对局部海面来说这个近似成立，但星球整体是曲面，所以它不是严格的球面反射或光线追踪反射。

### Q8：大气散射是怎么来的？

回答：

> shader 里对视线和大气球壳求交，然后沿视线采样积分。Rayleigh 散射模拟空气分子导致的蓝天，Mie 散射模拟气溶胶和太阳附近的亮斑。密度根据高度指数衰减。

### Q9：数据怎么从 CPU 传到 GPU？

回答：

> CPU 生成六个 face 的高度、温度、湿度、生物群系和侵蚀数据，然后 `PlanetRenderer::updateProceduralTextures` 把这些数据上传到 OpenGL texture array。shader 根据当前 face 和 UV 坐标采样对应 layer。

### Q10：怎么证明不是贴图假效果？

回答：

> 可以打开 debug mask，看高度、温度、湿度、生物群系和侵蚀 mask 的中间结果。还可以调侵蚀、海平面、海洋和大气参数，观察几何、水面和颜色实时变化。

## 14. 推荐现场演示流程

1. 启动程序，先展示完整星球视角。
2. 拉近地表，展示 LOD 和 tessellation 后的地形细节。
3. 打开 terrain debug mask，依次展示高度、温度、湿度、生物群系、侵蚀 mask。
4. 调整 erosion 参数或切换侵蚀显示，说明 channel/flow/wear/deposition，再打开 Rivers 开关展示分形支流、河道颜色、高光和折射式扰动来自提取后的 channel/flow mask。
5. 移动到海岸线，展示海洋透明、水深颜色变化和岸线效果。
6. 打开/关闭 reflection、refraction、Fresnel 或 planar targets，展示水面反射折射差异。
7. 调整 ocean wind、amplitude 等参数，展示 FFT 海浪变化。
8. 调整 atmosphere 参数，展示天空颜色、地平线和太阳方向散射变化。
9. 打开 performance 面板，说明 patch 数、draw call 和各 pass 耗时。

## 15. 不要过度声称的内容

答辩时建议明确区分“已实现”和“未完整实现”，这样更可信：

| 不要这么说 | 推荐说法 |
| --- | --- |
| 实现了完整真实地球级气候模拟 | 实现了基于纬度、海拔、噪声和水域修正的程序化气候近似 |
| 实现了完整物理海洋 | 实现了实时 FFT ocean 频谱波浪 |
| 实现了光线追踪反射 | 实现了基于 FBO 的 planar reflection/refraction |
| 实现了完整 texture splatting | 实现了基于生物群系权重的 procedural material blending |
| 实现了 GPU 侵蚀模拟 | 实现了 CPU 网格近似液压侵蚀和热侵蚀 |
| 实现了程序化植被系统 | 项目目前没有植被系统 |

## 16. 最短版答辩总结

如果老师只给很短时间，可以这样总结：

> 这个项目实现了一个实时程序化星球。地形部分使用 cube-sphere，把六个 cube face 转成球面，CPU 四叉树负责大范围 LOD，GPU tessellation 负责 patch 内细分。高度由 gradient noise 和 fBM 生成，并结合大陆、山脉、海底等地貌因子。温度和湿度根据纬度、海拔、水域距离和噪声生成，再决定生物群系权重。项目还做了 CPU 简化液压侵蚀和热侵蚀，输出 channel、flow、wear、deposition mask，并从这些 mask 中提取主干和支流河网给 shader 渲染细河流、湿润岸线、高光和折射式扰动。海洋使用 FFT 频谱生成动态波浪，并通过 reflection/refraction FBO、深度水色混合和 Fresnel 实现水面效果。天空部分使用大气球壳和 Rayleigh/Mie 散射近似。所有中间数据都可以通过 ImGui debug view 和性能面板验证。
