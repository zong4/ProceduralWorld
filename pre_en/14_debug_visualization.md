# 14. Debug Views and Performance UI

## Matching Images
- ../images_en/08_frame_debug.svg

## Goal
ImGui exposes parameters, visualization modes and performance counters.

## Key Code Locations
- src/main.cpp
- src/PlanetRenderer.cpp

## Explanation Flow
1. Render panel exposes terrain, ocean, atmosphere and cloud parameters
2. User switches render modes or debug overlays
3. Renderer updates shader uniforms
4. Hydrology, erosion and material layers can be visualized
5. Performance panel shows frame timing and visible regions
6. Ocean, cloud and baked chunk stats give feedback
7. Feature overlay code exists
8. Initial DEM path and main render do not fully connect overlay yet

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
> Hello, on this slide I will talk about **Debug Views and Performance UI**.

> The goal is simple: ImGui exposes parameters, visualization modes and performance counters.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **Render panel exposes terrain, ocean, atmosphere and cloud parameters**. This step prepares the next part.

> Second, the step is: **User switches render modes or debug overlays**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Renderer updates shader uniforms**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Hydrology, erosion and material layers can be visualized**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Performance panel shows frame timing and visible regions**. This step prepares the next part.

> Sixth, the step is: **Ocean, cloud and baked chunk stats give feedback**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Feature overlay code exists**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Initial DEM path and main render do not fully connect overlay yet**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> This module is useful during a live presentation because it exposes intermediate data, not just the final image.

> The render panel controls terrain, hydrology debug, ocean, atmosphere, clouds and camera quality settings.

> Debug views can show rivers, erosion, material information and LOD behavior.

> The performance panel reports frame time, visible chunks, ocean patches and cloud settings.

> Be accurate about feature overlays: the code exists, but the initial DEM generation path and current main render path do not fully enable it.

## Possible Follow-up Answer
This is a good slide to show engineering honesty: implemented systems, debug support and current limitations.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.