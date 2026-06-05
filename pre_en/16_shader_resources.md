# 16. Shader Compilation and Resource Management

## Matching Images
- ../images_en/28_flow_16_shader_resources.svg

## Goal
Shaders, GPU resources and render passes are initialized and managed centrally.

## Key Code Locations
- src/PlanetRenderer.cpp
- shaders/
- xmake.lua

## Explanation Flow
1. xmake copies shaders and assets
2. PlanetRenderer::initialize() loads shader files
3. Compile vertex, fragment and tessellation shaders
4. Link programs and check errors
5. Create VAO, VBO, textures and FBOs
6. Bind the required program for each pass
7. Set uniforms and texture slots
8. Release GPU resources on shutdown

## Speaking Script
> This module is basic engineering, but it is necessary for every visual system in the project.

> xmake copies shader and asset folders into the build output so runtime paths work.

> PlanetRenderer compiles and links shader programs for terrain, ocean, atmosphere and debug passes.

> It also creates textures, buffers, vertex arrays and framebuffers.

> Each render pass binds its own program, textures and uniform values. Compilation logs help locate GLSL errors.

## Possible Follow-up Answer
This slide shows that the project handles real graphics-program resource lifecycle, not just algorithms.