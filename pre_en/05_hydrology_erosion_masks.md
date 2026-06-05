# 05. Hydrology, Erosion and River Masks

## Matching Images
- ../images_en/04_hydrology_erosion.svg

## Goal
Downhill flow, stream power and thermal diffusion create channels and erosion traces.

## Key Code Locations
- src/PlanetProceduralData.cpp

## Explanation Flow
1. Sort texels by height
2. Find a downstream receiver for each texel
3. Accumulate drainage / flow
4. Compute streamPower from slope and drainage
5. Carve channels into the terrain
6. Write channel, wear, deposition and flow maps
7. Smooth steep slopes with thermal diffusion
8. Compute water depth, shorelines and moisture effects

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
> Hello, on this slide I will talk about **Hydrology, Erosion and River Masks**.

> The goal is simple: Downhill flow, stream power and thermal diffusion create channels and erosion traces.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **Sort texels by height**. This step prepares the next part.

> Second, the step is: **Find a downstream receiver for each texel**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Accumulate drainage / flow**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Compute streamPower from slope and drainage**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Carve channels into the terrain**. This step prepares the next part.

> Sixth, the step is: **Write channel, wear, deposition and flow maps**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Smooth steep slopes with thermal diffusion**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Compute water depth, shorelines and moisture effects**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> This module makes the terrain look shaped by water instead of being pure noise.

> The code sorts texels by height and sends water from higher cells to lower receivers.

> Drainage accumulates downstream. When both drainage and slope are high, stream power becomes strong enough to cut channels.

> The result is written into channel, flow, wear and deposition maps, which can be inspected in debug views.

> Thermal diffusion smooths overly steep slopes, while water depth and shoreline data support coastal and moisture effects.

## Possible Follow-up Answer
A simple way to explain it is: first decide where water flows, then decide how strongly water cuts the terrain.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.