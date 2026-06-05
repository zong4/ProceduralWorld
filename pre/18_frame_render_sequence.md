# 18. 每帧渲染顺序

## 对应图片
- ../images/11_frame_render_sequence.svg
- ../images/30_flow_18_frame_render_sequence.svg

## 讲解目标
把一次 frame 拆成反射折射、地形、海洋、大气和 UI。

## 关键代码位置
- src/PlanetRenderer.cpp
- src/main.cpp

## 讲解流程
1. 主循环处理输入和时间
2. 更新相机矩阵和 renderer 参数
3. 构建可见地形 chunk
4. 构建可见 ocean patch
5. 绘制 reflection/refraction FBO
6. 绘制主场景地形
7. 绘制海洋和大气云层
8. 绘制 ImGui UI 并交换缓冲

## 口播稿
> 这页可以作为前面所有模块的汇总，说明它们在一帧里如何协作。

> 每帧开始先处理输入和时间，更新相机。

> 渲染器根据相机选择可见地形 chunk 和海洋 patch。

> 海洋需要反射和折射，所以主画面前会先做离屏 pass。

> 之后绘制地形、海洋、大气和云层，最后绘制 ImGui。

> 这样讲可以让老师看到：生成模块负责准备数据，渲染模块负责按顺序把数据组织成最终画面。

## 老师可能追问时的回答
这页适合放在答辩中后段，用来把前面的模块重新串起来。