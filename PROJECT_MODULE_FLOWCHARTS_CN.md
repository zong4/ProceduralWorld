# ProceduralWorld 功能模块流程图说明

这份文档用于答辩讲解。项目本质上是一个基于 C++17、OpenGL 4.1、GLFW、GLAD、GLM、ImGui 和 xmake 的实时程序化星球渲染系统。当前代码的主线是：先在 CPU 端生成六个 cube face 的 DEM/高度场数据，再烘焙成地形 chunk 和多张 GPU texture array，运行时按相机位置做可见性裁剪、绘制地形、海洋、反射折射、大气和体积云。

需要注意：当前 `PlanetProceduralData::generate()` 会直接进入 `generateDemPrototype()` 并返回，所以实际运行的是 DEM 原型生成路径。后面保留的旧版分模块生成代码目前不会执行。

## 1. 项目总体流程

主要代码位置：

| 模块 | 代码 |
| --- | --- |
| 程序入口、窗口、输入、ImGui | `src/main.cpp` |
| 星球渲染主流程 | `src/PlanetRenderer.cpp`, `include/PlanetRenderer.h` |
| CPU 程序化星球数据 | `src/PlanetProceduralData.cpp`, `include/PlanetProceduralData.h` |
| FFT 海洋 | `src/FFTOcean.cpp`, `include/FFTOcean.h` |
| 地形、海洋、大气 shader | `shaders/` |
| 构建脚本 | `xmake.lua` |
| 高度诊断工具 | `tools/TerrainHeightDiagnostics.cpp` |

```mermaid
flowchart TD
    A["启动程序 main()"] --> B["初始化 GLFW / OpenGL / GLAD"]
    B --> C["初始化 ImGui"]
    C --> D["PlanetRenderer::initialize()"]
    D --> E["加载 shader、网格、FFT 海洋、大气 LUT 资源"]
    E --> F{"是否加载到本地 session/cache"}
    F -- 是 --> G["loadSession() + loadCache()"]
    G --> H["renderer.setProceduralData() 上传 GPU 数据"]
    H --> I["进入 Render 阶段"]
    F -- 否 --> J["进入 ProceduralSetup 阶段"]
    J --> K["用户调参数并点击 Generate Planet"]
    K --> L["后台线程 PlanetProceduralData::generate()"]
    L --> M["主线程 finishPlanetGeneration()"]
    M --> H
    I --> N["每帧处理输入、更新相机"]
    N --> O["renderer.render()"]
    O --> P["绘制地形、海洋、大气、UI"]
    P --> N
```

讲解要点：

- `main.cpp` 管理应用状态、输入、UI、生成线程和本地 session/cache。
- `PlanetRenderer` 只在主线程持有 OpenGL 资源，后台线程只生成 CPU 数据。
- `xmake.lua` 会把 `shaders/` 和 `assets/` 复制到构建输出目录。

## 2. 应用状态与生成线程

当前项目分三种工作流阶段：

| 阶段 | 含义 |
| --- | --- |
| `ProceduralSetup` | 参数设置界面，还没有进入实时渲染 |
| `Generating` | 后台线程生成星球数据 |
| `Render` | 数据已经上传 GPU，每帧实时渲染 |

```mermaid
flowchart TD
    A["ProceduralSetup 参数界面"] --> B["用户点击 Generate Planet"]
    B --> C["startPlanetGeneration()"]
    C --> D["复制 proceduralSettings 到 pendingGenerationSettings"]
    D --> E["std::async 后台生成 PlanetProceduralData"]
    E --> F["progressCallback 写入 atomic 进度"]
    F --> G["主循环持续刷新 ImGui 进度条"]
    G --> H{"future 是否 ready"}
    H -- 否 --> G
    H -- 是 --> I["finishPlanetGeneration()"]
    I --> J["renderer.settings() = 生成参数"]
    J --> K["renderer.setProceduralData()"]
    K --> L["生成纹理、烘焙网格上传 GPU"]
    L --> M["保存 session/cache"]
    M --> N["进入 Render 阶段"]
```

讲解要点：

- 后台线程不能直接创建 OpenGL texture 或 buffer，因为 OpenGL context 在主线程。
- 因此生成完成后，主线程通过 `renderer.setProceduralData()` 上传 GPU 资源。
- `saveSession()` 会保存 UI 参数和二进制 procedural cache；下次启动可直接加载。

## 3. DEM 程序化地形生成

实际入口：`PlanetProceduralData::generate()` -> `generateDemPrototype()`。

```mermaid
flowchart TD
    A["PlanetProceduralData::generate()"] --> B["clamp faceResolution"]
    B --> C["generateDemPrototype()"]
    C --> D["为 6 个 cube face 分配高度、水深、侵蚀、温湿度等数组"]
    D --> E["遍历每个 face 的每个 texel"]
    E --> F["cube face UV -> sphereDir"]
    F --> G["多层 fBM / ridgedFbm 生成大陆、山脉、省域、山脊"]
    G --> H["计算 height、uplift、landMask、preErosionHeight"]
    H --> I["fixCubeFaceSeams() 平滑六面体接缝"]
    I --> J["多轮水流/侵蚀 pass"]
    J --> K["按高度排序，计算下游 receiver 和 drainage"]
    K --> L["streamPower 切割河道，记录 channel/slope/wear"]
    L --> M["thermal diffusion 平滑山坡"]
    M --> N["computeWaterClimateFields() 计算水深、岸线、温度、湿度"]
    N --> O["写入 channel/flow/wear/deposition/domainWeight 等调试层"]
    O --> P["fixCubeFaceSeams() 最终接缝融合"]
    P --> Q["buildTerrainChunks() 生成离线地形块"]
    Q --> R["generated_ = true"]
```

讲解要点：

- 星球不是平面地形，而是六个 cube face 映射到球面。
- 高度值是归一化地形高度，最终几何半径为 `planetRadius + height * terrainHeightScale`。
- `uplift` 表示造山/隆升区域，后面影响网格密度、材质和调试 mask。
- 当前 DEM 路径会生成河道相关 mask，但初次生成末尾会清空并跳过 `terrainFeatureSegments_`；cache 加载时会调用 `buildTerrainFeatureSegments()` 重建特征线。

## 4. 六面体星球与接缝处理

代码位置：

- `PlanetProceduralData::cubeSphereDirection()`
- `PlanetProceduralData::neighborCell()`
- `PlanetProceduralData::fixCubeFaceSeams()`
- `shaders/planet_sampling.glsl`

```mermaid
flowchart TD
    A["faceIndex + face UV"] --> B["FaceBasis normal / axisU / axisV"]
    B --> C["cubePoint = normal + u * axisU + v * axisV"]
    C --> D["normalize(cubePoint)"]
    D --> E["得到 sphereDir"]
    E --> F["采样或生成高度"]
    F --> G["sphereDir * (planetRadius + height * scale)"]
    G --> H["最终球面地形顶点"]

    I["跨 face 边界采样"] --> J["neighborCell() 映射相邻面"]
    J --> K["fixCubeFaceSeams() 对边界值取平均"]
    K --> L["减少六个面边缘裂缝"]
```

讲解要点：

- 这样避免经纬球在两极三角形密集和 UV 拉伸严重的问题。
- 六个面之间的高度、温湿度、侵蚀、biome/domain 权重都会做 seam reconcile。

## 5. 水文、侵蚀和河道 mask

代码位置：

- `generateDemPrototype()`
- `computeWaterClimateFields()`
- `terrain_surface.glsl`

```mermaid
flowchart TD
    A["初始高度场"] --> B["每个格点寻找更低邻居"]
    B --> C["建立 receiver 和 receiverWeights"]
    C --> D["按高度从高到低累积 drainage"]
    D --> E["flow = drainage / maxDrainage"]
    E --> F["channel = smoothstep(flow)"]
    F --> G["streamPower = flow * slope * land"]
    G --> H["carve 河道下切"]
    H --> I["deposition 低坡沉积"]
    I --> J["更新 height"]
    J --> K["thermal diffusion 平滑山坡"]
    K --> L["输出 channelMask / flowMask / wearMask / depositionMask"]
    L --> M["上传为 proceduralErosionMaskTexture RGBA"]
    M --> N["terrain_surface.glsl 渲染湿润河道、河岸、高光、折射式扰动"]
```

讲解要点：

- 这里不是完整流体模拟，而是适合程序化地形的网格近似侵蚀。
- `channelMask` 和 `flowMask` 不只是显示颜色，也会参与材质混合、湿润河岸和河道高光。
- UI 中的 Hydrology Debug 控制的是湿润通道/河道视觉表现，不会实时重算 CPU 地形。

## 6. 温湿度、材质和地表分类

代码位置：

- `computeWaterClimateFields()`
- `PlanetProceduralData::FaceData`
- `shaders/terrain_surface.glsl`
- `shaders/terrain_lighting.glsl`

```mermaid
flowchart TD
    A["sphereDir + height"] --> B["samplePlanetBase()"]
    B --> C["计算 waterDepth / shoreMask"]
    B --> D["计算 temperature"]
    B --> E["计算 moisture"]
    C --> F["domainWeight / biomeWeight / debug layers"]
    D --> F
    E --> F
    F --> G["上传 texture array"]
    G --> H["terrain_surface.glsl samplePlanet()"]
    H --> I["读取高度、水深、温度、湿度、侵蚀、biome/domain 权重"]
    I --> J["综合高度、坡度、温湿度、侵蚀和噪声"]
    J --> K["混合草地、森林、沙漠、岩石、雪、湿地、海床等颜色"]
    K --> L["加入微表面 normal、粗糙度、河道 specular"]
    L --> M["terrain_lighting.glsl 计算光照和 tone mapping"]
```

讲解要点：

- 当前是 procedural material blending，不是加载多套真实地表贴图做传统 texture splatting。
- 地表颜色来自多因子混合：高度、坡度、温度、湿度、海岸、水深、侵蚀和噪声。
- `renderMode` 可切换 Shaded、Unshaded、HeightMap、Normals、Material 等调试视图。

## 7. 地形 chunk 烘焙与 Baked LOD

代码位置：

- `PlanetProceduralData::buildTerrainChunks()`
- `PlanetRenderer::BakedTerrainMesh::upload()`
- `PlanetRenderer::buildVisibleBakedChunks()`
- `PlanetRenderer::drawBakedTerrainPass()`

```mermaid
flowchart TD
    A["DEM 高度场和 mask"] --> B["buildTerrainChunks()"]
    B --> C["按 cube face 划分固定深度 chunk"]
    C --> D["采样 chunk 内高度、meshDensity、geometricError、featureMask"]
    D --> E["生成 chunk 顶点：sphereDir、normal、uv、height、featureWeight"]
    E --> F["生成三角形索引，按 feature/height 选择对角线"]
    F --> G["terrainChunks_"]
    G --> H["BakedTerrainMesh::upload()"]
    H --> I["创建 VBO / IBO / VAO"]
    I --> J["为每个 chunk 记录 center、radius、LOD index range"]
    J --> K["每帧 buildVisibleBakedChunks()"]
    K --> L["视锥裁剪 + 地平线裁剪"]
    L --> M["根据屏幕投影半径选择 full / mid / low LOD"]
    M --> N["drawBakedTerrainPass() 绘制可见 chunk"]
```

讲解要点：

- 当前地形主路径是 baked chunks，不是每帧对地形 patch 做 tessellation。
- 性能面板显示 `Baked chunks: visible / total` 和 full/mid/low LOD 数量。
- `drawWireOverlayPass()` 中的 Baked LOD 模式可显示地形块线框。

## 8. CPU 数据到 GPU 的上传

代码位置：`PlanetRenderer::setProceduralData()`。

```mermaid
flowchart TD
    A["PlanetProceduralData::faces()"] --> B["展开 6 个 face 为 texture array 数据"]
    B --> C["height -> GL_R32F texture array"]
    B --> D["waterDepth -> GL_R32F texture array"]
    B --> E["temperature / moisture -> GL_R32F texture array"]
    B --> F["channel/flow/wear/deposition -> RGBA32F erosion texture"]
    B --> G["biomeWeightA/B/domainWeight -> RGBA32F texture array"]
    C --> H["shader 通过 face + sphereDir 采样"]
    D --> H
    E --> H
    F --> H
    G --> H
    B --> I["构建 water/shore 前缀和"]
    I --> J["LOD 阶段 O(1) 判断 patch 是否含水/海岸"]
    B --> K["BakedTerrainMesh::upload() 上传地形网格"]
    B --> L["FeatureSegmentMesh::upload() 上传特征线网格"]
```

讲解要点：

- texture array 的 layer 对应 6 个 cube face。
- 侵蚀数据打包为 RGBA：R=channel，G=flow，B=wear，A=deposition。
- 前缀和用于快速判断海洋 patch 是否需要绘制或强制提高海岸 LOD。

## 9. 海洋 patch LOD 与球面海面

代码位置：

- `buildVisibleOceanPatches()`
- `collectVisibleOceanPatches()`
- `shouldSplitNode()`
- `analyzePatchWaterCoverage()`
- `shaders/ocean.tesc`, `shaders/ocean.tese`

```mermaid
flowchart TD
    A["每帧 render()"] --> B["extractFrustum()"]
    B --> C["对 6 个 cube face 从 root quadtree 遍历"]
    C --> D["computeNodeBounds()"]
    D --> E{"视锥外或地平线后方"}
    E -- 是 --> F["剔除节点"]
    E -- 否 --> G["analyzePatchWaterCoverage()"]
    G --> H{"海岸或水陆混合"}
    H -- 是 --> I["强制细分到 shore minimum depth"]
    H -- 否 --> J["shouldSplitNode() 根据屏幕投影半径判断"]
    I --> K["递归 2x2 子节点"]
    J -- 需要细分 --> K
    J -- 不细分 --> L["输出 OceanPatch"]
    L --> M["过滤无水 patch"]
    M --> N["ocean.tesc/tese 进行 GPU tessellation"]
    N --> O["cube face UV -> sphereDir -> seaLevelRadius 球面"]
```

讲解要点：

- 海洋仍使用 quadtree patch + GPU tessellation。
- 海岸线区域会更细，因为水陆交界最容易暴露几何和材质问题。
- `updateOceanTessellationBudget()` 会根据 patch 数动态调节预算，避免水面 patch 过多。

## 10. FFT 海浪

代码位置：

- `FFTOcean::initialize()`
- `FFTOcean::buildInitialSpectrum()`
- `FFTOcean::update()`
- `FFTOcean::uploadTextures()`
- `shaders/ocean.tese`, `shaders/ocean.frag`

```mermaid
flowchart TD
    A["FFTOcean::initialize()"] --> B["创建 height / normal / displacement / folding 纹理"]
    B --> C["configureCascades() 多级波浪 cascade"]
    C --> D["buildInitialSpectrum()"]
    D --> E["Phillips spectrum 生成 h0(k)"]
    E --> F["每帧或按 frame stride 调用 update(time)"]
    F --> G["h(k,t)=h0(k)e^iwt + conj(h0(-k))e^-iwt"]
    G --> H["生成水平 displacement spectrum"]
    H --> I["2D inverse FFT"]
    I --> J["空间域 height / displacement"]
    J --> K["中心差分重建 normal"]
    K --> L["glTexSubImage2D 上传动态纹理"]
    L --> M["ocean.tese 采样高度和位移"]
    M --> N["ocean.frag 采样 normal 和 detail normal"]
```

讲解要点：

- FFT ocean 是频域海浪近似，不是 Navier-Stokes 流体求解。
- 多个 cascade 用不同 patchLength、振幅和速度覆盖大浪、中浪、细浪。
- shader 用 triplanar 投影把 2D FFT 纹理贴到球面海洋上，减少球面 UV 拉伸。

## 11. 海洋材质、反射、折射和水深混合

代码位置：

- `drawReflectionRefractionPasses()`
- `drawOceanPass()`
- `shaders/ocean.frag`

```mermaid
flowchart TD
    A["render()"] --> B["drawReflectionRefractionPasses()"]
    B --> C{"海洋开启且存在可见海洋 patch"}
    C -- 否 --> D["跳过 planar targets"]
    C -- 是 --> E["按相机海拔和 Auto Distance LOD 计算权重"]
    E --> F["创建或复用 reflection/refraction FBO"]
    F --> G["reflection pass：镜像相机 + clip plane 绘制地形"]
    F --> H["refraction pass：正常相机 + clip plane 绘制地形和深度"]
    G --> I["drawOceanPass()"]
    H --> I
    I --> J["绑定 reflection/refraction/depth/FFT/水深纹理"]
    J --> K["ocean.frag"]
    K --> L["屏幕 UV 采样反射与折射颜色"]
    K --> M["线性化 refraction depth 估算水柱厚度"]
    K --> N["采样 proceduralWaterDepth 判断真实覆盖水域"]
    L --> O["Fresnel 混合反射和折射"]
    M --> P["浅水/深水颜色混合"]
    N --> Q["岸线 alpha 和 discard"]
    O --> R["GGX 高光 + SSS 浪尖透光 + 大气透视"]
    P --> R
    Q --> R
    R --> S["最终海水颜色和透明度"]
```

讲解要点：

- 反射折射是 planar approximation，以海平面高度为镜像基准。
- 对球面星球来说这不是严格曲面光线追踪，但实时效果和性能更合适。
- 水色同时使用屏幕深度和程序化水深，近处边缘更稳定，远处覆盖更完整。

## 12. 大气散射 LUT 与天空

代码位置：

- `AtmosphereLut::create()`
- `computeAtmosphereLutSignature()`
- `precomputeAtmosphereLuts()`
- `drawAtmospherePass()`
- `shaders/atmosphere_*.frag`
- `shaders/atmosphere.frag`

```mermaid
flowchart TD
    A["大气参数变化"] --> B["computeAtmosphereLutSignature()"]
    B --> C{"signature 是否变化"}
    C -- 否 --> D["复用已有 LUT"]
    C -- 是 --> E["precomputeAtmosphereLuts()"]
    E --> F["transmittance LUT"]
    F --> G["irradiance LUT"]
    G --> H["多阶 scattering delta"]
    H --> I["accumulate 到 3D scattering LUT"]
    I --> J["缓存 signature"]
    D --> K["drawAtmospherePass()"]
    J --> K
    K --> L["复制当前场景深度"]
    L --> M["fullscreen triangle"]
    M --> N["atmosphere.frag 重建视线"]
    N --> O["采样 irradiance/scattering LUT"]
    O --> P["根据视线、太阳方向、地平线和场景深度输出大气颜色"]
```

讲解要点：

- 当前大气是 LUT-backed scattering，不是简单天空盒图片。
- 大气参数改变才重建 LUT，避免每帧重复昂贵预计算。
- `sceneDepthTexture` 用于让大气和已有地形/海洋深度关系更自然。

## 13. 程序化体积云

新增重点代码位置：

- `PlanetRenderSettings` 中 `renderClouds`、`cloudCoverage`、`cloudStepCount` 等参数
- `drawAtmospherePass()` 传入云层 uniform
- `shaders/atmosphere.frag`
- `main.cpp` 的 Clouds UI 面板

```mermaid
flowchart TD
    A["Clouds UI 参数"] --> B["写入 PlanetRenderSettings"]
    B --> C["drawAtmospherePass() 传 uniform"]
    C --> D["atmosphere.frag 中 renderClouds 判断"]
    D --> E{"云层开启且 opacity > 0"}
    E -- 否 --> F["只渲染大气散射"]
    E -- 是 --> G["根据 cloudHeight / cloudThickness 计算云壳半径"]
    G --> H["cloudShellInterval() 求视线穿过云层区间"]
    H --> I["raymarchCloudVolume()"]
    I --> J["cloudDensityAtWorld()"]
    J --> K["fBM + Worley 噪声生成云团、边缘破碎、体积细节"]
    I --> L["cloudLightOpticalDepth() 沿太阳方向采样自阴影"]
    K --> M["累积颜色和 alpha"]
    L --> M
    M --> N["与大气颜色混合"]
```

讲解要点：

- 云不是贴图，而是在大气 pass 中对云壳做 raymarch。
- 形状由 fBM、Worley、coverage、sharpness、scale、speed 控制。
- `cloudStepCount` 和 `cloudLightStepCount` 是质量/性能权衡参数。

## 14. 调试、可视化和性能面板

代码位置：

- `drawRenderPanel()`
- `drawPerformancePanel()`
- `drawWireOverlayPass()`
- `terrain_debug.glsl`
- `feature_segments.vert/frag`

```mermaid
flowchart TD
    A["ImGui Render Controls"] --> B["切换 renderMode"]
    B --> C["Shaded / Unshaded / HeightMap / Normals / Material"]
    A --> D["切换 wireMode"]
    D --> E["Ocean wire / Baked LOD wire / MountainMask"]
    A --> F["Hydrology Debug"]
    F --> G["调整 wet channel 可见性、宽度、高光、折射扰动"]
    A --> H["Clouds / Ocean / Atmosphere 参数"]
    H --> I["实时影响 shader uniform"]
    A --> J["Ctrl+1 打开 Performance Monitor"]
    J --> K["显示 culling、FFT、reflection/refraction、terrain/ocean/atmosphere 耗时"]
    K --> L["显示 baked chunk LOD、ocean patch、估算三角形数"]
```

特征线 overlay 当前状态：

```mermaid
flowchart TD
    A["buildTerrainFeatureSegments()"] --> B["根据 channel/shore/ridge/wear 生成线段"]
    B --> C["FeatureSegmentMesh::upload()"]
    C --> D["drawFeatureOverlayPass() 可按 Rivers/Coast/Ridges/Erosion 绘线"]
    D --> E["UI 已提供 Feature Overlay 选项"]
    E --> F["当前 render() 主流程尚未调用 drawFeatureOverlayPass()"]
    A --> G["当前 generateDemPrototype() 初次生成结束时会清空并跳过特征线"]
    G --> H["loadCache() 会重新 buildTerrainFeatureSegments()"]
```

讲解建议：

- 已实现的稳定调试能力：render mode、wire mode、性能面板、湿润河道 overlay。
- 特征线 overlay 代码已具备，但当前主帧路径和 DEM 初次生成路径没有完全接入；如果演示需要，需要在 `render()` 中调用 `drawFeatureOverlayPass()` 并避免初次生成时清空线段。

## 15. 输入和相机控制

代码位置：

- `FlyCamera.h`
- `onMouseScrolled()`
- `onMouseMoved()`
- `onKeyPressed()`
- `handleKeyboardMovement()`

```mermaid
flowchart TD
    A["用户输入"] --> B{"ImGui 是否捕获输入"}
    B -- 是 --> C["交给 ImGui"]
    B -- 否 --> D{"当前是否 Render 阶段"}
    D -- 否 --> E["Esc 退出 / Tab 切 UI"]
    D -- 是 --> F["键盘 W/A/S/D/Q/E"]
    F --> G["绕星球中心轨道旋转相机"]
    D --> H["鼠标滚轮"]
    H --> I["调整 cameraOrbitDistance"]
    D --> J["左键拖动"]
    J --> K["旋转星球 modelMatrix"]
    D --> L["数字键"]
    L --> M["切换 renderMode、wireMode、地形/海洋可见性"]
    G --> N["updateOrbitCamera() 锁定看向星球中心"]
    I --> N
```

讲解要点：

- 当前不是普通自由飞行，而是围绕星球中心的轨道观察模式。
- FOV 固定，滚轮改变轨道距离，保证观察星球时比例稳定。

## 16. Shader 编译和资源管理

代码位置：

- `ShaderProgram.h`
- `PlanetRenderer::initialize()`
- `xmake.lua`

```mermaid
flowchart TD
    A["PlanetRenderer::initialize()"] --> B["创建 ShaderProgram"]
    B --> C["ShaderProgram::expandIncludes()"]
    C --> D["展开 #include 的 GLSL 文件"]
    D --> E["compileShader()"]
    E --> F["linkProgram()"]
    F --> G["缓存 uniform location"]
    A --> H["创建 terrainMesh / atmosphere mesh / fullscreen VAO"]
    A --> I["FFTOcean::initialize()"]
    A --> J["AtmosphereLut::create()"]
    K["xmake after_build"] --> L["复制 shaders/ 到输出目录"]
    K --> M["复制 assets/ 到输出目录"]
```

讲解要点：

- shader 支持轻量 `#include`，所以 `terrain_chunk.frag` 可以复用 `terrain_surface.glsl`、`terrain_lighting.glsl` 等。
- uniform location 有缓存，减少每帧大量 draw call 中的 `glGetUniformLocation` 开销。

## 17. 高度诊断工具

代码位置：`tools/TerrainHeightDiagnostics.cpp`，xmake target 为 `TerrainHeightDiagnostics`。

```mermaid
flowchart TD
    A["运行 TerrainHeightDiagnostics"] --> B{"mode"}
    B -- raw --> C["直接调用 PlanetTerrainGenerator::terrainHeight()"]
    C --> D["统计原始高度分布"]
    D --> E["导出 terrain_height_raw.pgm"]
    B -- default --> F["生成 no_erosion 星球"]
    F --> G["统计高度、水域、岸线、chunk、局部峰值等"]
    G --> H["导出 height/channel/flow/wear/uplift/slope PGM"]
    B -- full --> I["额外生成带侵蚀版本"]
    I --> J["对比侵蚀前后数据"]
```

讲解要点：

- 这个工具用于证明程序化地形不是只看画面，而是能导出和分析中间数据。
- 可以辅助回答老师关于高度分布、侵蚀效果和局部噪声是否合理的问题。

## 18. 推荐答辩讲解顺序

```mermaid
flowchart TD
    A["1. 先说明项目目标：实时程序化星球"] --> B["2. 展示总体架构：CPU 生成 + GPU 渲染"]
    B --> C["3. 讲 cube-sphere：六面体映射到球面"]
    C --> D["4. 讲 DEM 生成：大陆、山脉、侵蚀、水文 mask"]
    D --> E["5. 讲数据上传：texture array + baked chunks"]
    E --> F["6. 讲地形渲染：材质混合、调试视图"]
    F --> G["7. 讲海洋：quadtree patch + tessellation + FFT"]
    G --> H["8. 讲反射折射：两个 FBO + Fresnel + 水深"]
    H --> I["9. 讲大气和云：LUT scattering + raymarch cloud"]
    I --> J["10. 讲调试与性能：UI、LOD、耗时统计、诊断工具"]
```

## 19. 不要过度宣称的点

| 不建议说 | 更稳的说法 |
| --- | --- |
| 完整真实地球气候模拟 | 基于纬度、海拔、水域、噪声和水文修正的程序化近似 |
| 完整物理海洋 | 实时 FFT ocean 频域海浪近似 |
| 光线追踪反射 | 基于 FBO 的 planar reflection/refraction |
| 完整 texture splatting | 基于高度、坡度、温湿度、侵蚀和权重的 procedural material blending |
| GPU compute 侵蚀 | CPU 网格近似水文侵蚀和热扩散 |
| 特征线 overlay 已完整演示 | 已有提取和绘制代码，但当前主 render 未接入，初次 DEM 生成也会跳过线段 |

## 20. 一分钟总结版本

这个项目实现了一个实时程序化星球渲染系统。它用 cube-sphere 把星球拆成六个面，在 CPU 上生成 DEM 高度场、水深、岸线、侵蚀、河道、温湿度和材质权重，再上传成 OpenGL texture array，同时烘焙地形 chunk 用于实时渲染。运行时，地形使用 baked chunk LOD 和 shader 材质混合；海洋使用 quadtree patch、GPU tessellation 和 CPU FFT 生成的动态波浪纹理，并通过反射/折射 FBO、水深混合和 Fresnel 做水面效果；大气使用 LUT-backed scattering，云层在大气 pass 中做体积 raymarch。ImGui 提供生成参数、渲染参数、调试视图和性能统计，工具程序还能导出高度和侵蚀诊断图。
