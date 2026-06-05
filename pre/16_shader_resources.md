# 16. Shader 编译与资源管理

## 对应图片
- ../images/28_flow_16_shader_resources.svg

## 讲解目标
集中加载 shader、材质资源和 GPU 对象，失败时给出日志。

## 关键代码位置
- src/PlanetRenderer.cpp
- shaders/
- xmake.lua

## 讲解流程
1. xmake 构建时复制 shaders 和 assets
2. PlanetRenderer::initialize() 加载 shader 文件
3. 编译 vertex / fragment / tessellation shader
4. 链接 program 并检查错误
5. 创建 VAO / VBO / texture / FBO
6. 运行时绑定对应 program
7. 设置 uniform 和 texture slot
8. 程序退出时释放 GPU 资源

## 口播稿
> 这个模块偏工程基础，但很重要，因为所有视觉效果都依赖 shader 和 GPU 资源正确加载。

> xmake.lua 会把 shaders 和 assets 复制到输出目录，避免运行时找不到资源。

> PlanetRenderer 初始化时编译并链接多个 shader program，包括地形、海洋、大气和调试 pass。

> 创建纹理、缓冲、FBO 等资源后，每个渲染 pass 会绑定对应 program 和 uniform。

> 如果 shader 编译失败，日志可以帮助定位是哪一行 GLSL 出问题。

## 老师可能追问时的回答
这页不用讲太久，但能说明项目不是只写算法，也处理了实际图形程序的资源生命周期。