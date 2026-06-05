# 14. 调试、可视化与性能面板

## 对应图片
- ../images/12_debug_and_presentation.svg
- ../images/26_flow_14_debug_visualization.svg

## 讲解目标
用 ImGui 暴露参数和调试视图，帮助解释生成结果。

## 关键代码位置
- src/main.cpp
- src/PlanetRenderer.cpp

## 讲解流程
1. Render panel 展示地形、海洋、大气、云参数
2. 用户切换 render mode / debug overlay
3. renderer 更新 shader uniform
4. 显示 hydrology、erosion、material 等调试层
5. Performance panel 统计帧时间和可见块
6. Ocean / cloud / baked chunk 参数实时反馈
7. Feature overlay 代码存在
8. 当前初次 DEM 路径和主 render 还未完整接通 overlay

## 口播稿
> 这个模块适合答辩现场展示，因为它能证明项目不是只跑一次结果，而是可以调参数、看中间层。

> Render panel 里可以控制地形表面、水文调试、海洋、大气、云和相机质量参数。

> 调试视图能显示河道、侵蚀、材质、LOD 等信息，用来解释为什么某个区域会形成山脉、河流或海岸。

> Performance panel 则显示帧时间、可见 chunk、海洋 patch 和云质量等数据。

> 需要诚实说明：特征线 overlay 的构建代码存在，但当前初次 DEM 生成路径会跳过，主 render 也没有完整调用；cache 加载路径可以重建部分数据。

## 老师可能追问时的回答
这一页可以主动展示工程诚实性：哪些功能已实装，哪些是代码支持但当前路径未完全启用。