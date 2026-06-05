# 01. 项目总体流程

## 对应图片
- ../images/01_overall_pipeline.svg
- ../images/13_flow_01_project_overview.svg

## 讲解目标
从应用启动、CPU 程序化生成，到 GPU 实时渲染的完整链路。

## 关键代码位置
- src/main.cpp
- src/PlanetRenderer.cpp
- src/PlanetProceduralData.cpp
- xmake.lua

## 讲解流程
1. 启动 main() 创建窗口和 OpenGL 上下文
2. 初始化 ImGui 与 PlanetRenderer
3. 加载 shader、网格、海洋与大气资源
4. 尝试读取 session/cache
5. 无缓存时进入参数设置并生成星球
6. CPU 生成 DEM、气候、水文和材质数据
7. 主线程上传 texture array 与 baked mesh
8. 每帧绘制地形、海洋、大气、体积云和 UI

## 口播稿
> 这一页先讲项目的总目标：它不是静态模型，而是一个实时程序化星球渲染系统。

> 代码把工作拆成两层。CPU 端负责生成地形高度、水文、侵蚀、温度、湿度和材质 mask；GPU 端负责把这些数据变成实时画面。

> 主程序先初始化窗口、OpenGL、ImGui 和渲染器，然后优先尝试读取本地缓存。缓存存在时可以直接进入渲染；没有缓存时，用户在参数界面点击 Generate Planet 后启动后台生成。

> 生成完成后，真正的 OpenGL 上传一定回到主线程完成，因为纹理和缓冲对象依赖当前 OpenGL context。

> 最后进入 render loop，每帧根据相机位置做可见性判断，再依次绘制地形、海洋、大气、体积云和调试界面。

## 老师可能追问时的回答
老师如果问项目核心创新点，可以回答：重点是把程序化 DEM 星球、分块 LOD、FFT 海洋、大气散射和可调试 UI 组合成了一个完整的实时渲染管线。