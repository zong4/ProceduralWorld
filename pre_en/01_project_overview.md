# 01. Project Overview

## Matching Images
- ../images_en/01_system_overview.svg

## Goal
The complete path from application startup and CPU procedural generation to real-time GPU rendering.

## Key Code Locations
- src/main.cpp
- src/PlanetRenderer.cpp
- src/PlanetProceduralData.cpp
- xmake.lua

## Explanation Flow
1. Start main() and create the OpenGL window
2. Initialize ImGui and PlanetRenderer
3. Load shaders, meshes, ocean and atmosphere resources
4. Try to restore session/cache data
5. Without cache, enter the procedural setup screen
6. CPU generates DEM, climate, hydrology and material data
7. Main thread uploads texture arrays and baked meshes
8. Render terrain, ocean, atmosphere, clouds and UI every frame

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
> Hello, on this slide I will talk about **Project Overview**.

> The goal is simple: The complete path from application startup and CPU procedural generation to real-time GPU rendering.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **Start main() and create the OpenGL window**. This step prepares the next part.

> Second, the step is: **Initialize ImGui and PlanetRenderer**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Load shaders, meshes, ocean and atmosphere resources**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Try to restore session/cache data**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Without cache, enter the procedural setup screen**. This step prepares the next part.

> Sixth, the step is: **CPU generates DEM, climate, hydrology and material data**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Main thread uploads texture arrays and baked meshes**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Render terrain, ocean, atmosphere, clouds and UI every frame**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> This slide introduces the main goal: the project is not a static model, but a real-time procedural planet renderer.

> The work is separated into two layers. The CPU generates terrain height, hydrology, erosion, temperature, moisture and material masks. The GPU turns those datasets into the final real-time image.

> The program initializes the window, OpenGL, ImGui and the renderer, then tries to restore local cached data. If no cache is available, the user edits parameters and starts planet generation.

> Once generation is finished, OpenGL resource upload happens on the main thread, because textures and buffers depend on the active OpenGL context.

> The render loop then culls visible regions and draws terrain, ocean, atmosphere, volumetric clouds and debug UI.

## Possible Follow-up Answer
If asked about the core contribution, explain that the project integrates procedural DEM terrain, chunk LOD, FFT ocean, atmosphere scattering and an interactive debug UI into one renderer.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.