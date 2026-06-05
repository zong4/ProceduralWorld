# 18. Per-Frame Render Sequence

## Matching Images
- ../images_en/08_frame_debug.svg

## Goal
A frame is assembled from culling, offscreen passes, terrain, ocean, atmosphere and UI.

## Key Code Locations
- src/PlanetRenderer.cpp
- src/main.cpp

## Explanation Flow
1. Main loop processes input and time
2. Update camera matrices and renderer settings
3. Build visible terrain chunks
4. Build visible ocean patches
5. Render reflection/refraction FBOs
6. Draw main terrain scene
7. Draw ocean, atmosphere and clouds
8. Draw ImGui and swap buffers

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
> Hello, on this slide I will talk about **Per-Frame Render Sequence**.

> The goal is simple: A frame is assembled from culling, offscreen passes, terrain, ocean, atmosphere and UI.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **Main loop processes input and time**. This step prepares the next part.

> Second, the step is: **Update camera matrices and renderer settings**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Build visible terrain chunks**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Build visible ocean patches**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Render reflection/refraction FBOs**. This step prepares the next part.

> Sixth, the step is: **Draw main terrain scene**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Draw ocean, atmosphere and clouds**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Draw ImGui and swap buffers**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> This slide ties all modules together by showing what happens in one frame.

> The frame begins with input and time updates, then the camera matrices are refreshed.

> The renderer selects visible terrain chunks and ocean patches based on the camera.

> Ocean reflection and refraction are prepared with offscreen passes before the main scene.

> Then terrain, ocean, atmosphere and clouds are drawn, followed by the ImGui interface.

> This makes the distinction clear: generation prepares data, rendering organizes that data into the final image every frame.

## Possible Follow-up Answer
Use this slide in the middle or near the end to reconnect the individual modules.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.