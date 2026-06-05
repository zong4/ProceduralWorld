# 15. 输入与相机控制

## 对应图片
- ../images/27_flow_15_input_camera.svg

## 讲解目标
用键鼠控制轨道、移动和视角，使星球可交互观察。

## 关键代码位置
- src/main.cpp
- include/PlanetRenderer.h

## 讲解流程
1. GLFW 捕获键盘和鼠标事件
2. main.cpp 更新 camera state
3. 鼠标控制视角或轨道
4. 键盘控制移动、缩放和模式
5. 根据 delta time 平滑更新位置
6. 计算 view / projection matrix
7. 传入 PlanetRenderer
8. 各 shader 使用相机位置和矩阵渲染

## 口播稿
> 实时渲染项目必须能交互观察，否则很难展示星球尺度和细节。

> 输入模块主要由 GLFW 回调和 main loop 中的状态更新组成。

> 键盘和鼠标改变相机位置、方向或轨道参数，代码再根据 delta time 做平滑更新。

> 最终得到 view 和 projection matrix，传给 PlanetRenderer 和 shader。

> 相机位置还会影响地形 LOD、海洋 patch LOD、大气视角和 Fresnel 效果，所以它不仅是观察工具，也参与渲染决策。

## 老师可能追问时的回答
可以把相机讲成渲染系统的输入之一：它决定看哪里，也决定哪些资源需要画。