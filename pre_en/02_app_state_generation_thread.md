# 02. Application States and Generation Thread

## Matching Images
- ../images_en/01_system_overview.svg

## Goal
Three stages separate parameter editing, background generation and real-time rendering.

## Key Code Locations
- src/main.cpp

## Explanation Flow
1. ProceduralSetup shows the parameter UI
2. User clicks Generate Planet
3. startPlanetGeneration() copies current settings
4. std::async generates PlanetProceduralData in the background
5. progressCallback updates atomic progress values
6. The main loop keeps refreshing the progress bar
7. When the future is ready, finishPlanetGeneration() runs
8. setProceduralData() uploads GPU data and enters Render

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
> Hello, on this slide I will talk about **Application States and Generation Thread**.

> The goal is simple: Three stages separate parameter editing, background generation and real-time rendering.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **ProceduralSetup shows the parameter UI**. This step prepares the next part.

> Second, the step is: **User clicks Generate Planet**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **startPlanetGeneration() copies current settings**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **std::async generates PlanetProceduralData in the background**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **progressCallback updates atomic progress values**. This step prepares the next part.

> Sixth, the step is: **The main loop keeps refreshing the progress bar**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **When the future is ready, finishPlanetGeneration() runs**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **setProceduralData() uploads GPU data and enters Render**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> This module shows the engineering structure of the application. Planet generation can take time, so the main thread does not block directly.

> The background thread only computes CPU-side data such as height fields, hydrology and material weights. It does not create OpenGL objects.

> The main loop reads the future status and atomic progress values to keep the UI responsive.

> When generation is done, finishPlanetGeneration() hands the result to PlanetRenderer, and the main thread creates textures, buffers and vertex arrays.

> This design avoids OpenGL threading problems and keeps the program states easy to explain.

## Possible Follow-up Answer
The key point is that threading is used to keep the UI responsive and to keep all OpenGL resource creation on the main thread.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.