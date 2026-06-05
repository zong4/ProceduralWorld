# 16. Shader Compilation and Resource Management

## Matching Images
- ../images_en/05_gpu_lod.svg

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

## Simple Words
- renderer: the part that draws the scene
- shader: a small GPU program
- texture: image data used by the GPU
- data: information used by the program
- pass: one draw step
- cache: saved data that can be used again
- DEM: a height map of the planet
- FFT: a math tool that helps make waves
- LOD: use more detail near the camera and less detail far away
- FBO: an off-screen image used before the final image
- LUT: a table that is computed before rendering
- raymarch: sample many small points along a ray

## Read-Aloud Script
> Hello, on this slide I will talk about **Shader Compilation and Resource Management**.

> The goal is simple: Shaders, GPU resources and render passes are initialized and managed centrally.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **xmake copies shaders and assets**. This step prepares the next part.

> Second, the step is: **PlanetRenderer::initialize() loads shader files**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Compile vertex, fragment and tessellation shaders**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Link programs and check errors**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Create VAO, VBO, textures and FBOs**. This step prepares the next part.

> Sixth, the step is: **Bind the required program for each pass**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Set uniforms and texture slots**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Release GPU resources on shutdown**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> This module is basic engineering, but it is necessary for every visual system in the project.

> xmake copies shader and asset folders into the build output so runtime paths work.

> PlanetRenderer compiles and links shader programs for terrain, ocean, atmosphere and debug passes.

> It also creates textures, buffers, vertex arrays and framebuffers.

> Each render pass binds its own program, textures and uniform values. Compilation logs help locate GLSL errors.

## Possible Follow-up Answer
This slide shows that the project handles real graphics-program resource lifecycle, not just algorithms.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.