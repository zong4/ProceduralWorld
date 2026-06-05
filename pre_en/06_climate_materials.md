# 06. Climate, Materials and Surface Classification

## Matching Images
- ../images_en/03_dem_terrain.svg

## Goal
Height, latitude, water and erosion data become renderable surface materials.

## Key Code Locations
- src/PlanetProceduralData.cpp
- shaders/terrain.frag

## Explanation Flow
1. Estimate latitude from sphereDir
2. Reduce temperature by elevation
3. Increase moisture near water, rivers and coasts
4. Use slope to expose rock
5. Use erosion and deposition for color variation
6. Compute materialWeights
7. Upload data as GPU texture arrays
8. Blend grass, rock, snow and coast in terrain shader

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
> Hello, on this slide I will talk about **Climate, Materials and Surface Classification**.

> The goal is simple: Height, latitude, water and erosion data become renderable surface materials.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **Estimate latitude from sphereDir**. This step prepares the next part.

> Second, the step is: **Reduce temperature by elevation**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Increase moisture near water, rivers and coasts**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Use slope to expose rock**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Use erosion and deposition for color variation**. This step prepares the next part.

> Sixth, the step is: **Compute materialWeights**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Upload data as GPU texture arrays**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Blend grass, rock, snow and coast in terrain shader**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> The height field defines shape, while this module decides what the surface looks like.

> Temperature depends mainly on latitude and elevation. Moisture depends on water bodies, rivers and coastlines.

> Steep slopes expose more rock. High and cold areas can become snowy. Wet areas near water can look different from dry terrain.

> These values are packed into material weights and uploaded to the GPU.

> The terrain shader blends the final surface colors based on those weights.

## Possible Follow-up Answer
This is a good place to explain that the final look is controlled by more than height: climate, slope and hydrology all matter.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.