# 08. CPU-to-GPU Data Upload

## Matching Images
- ../images_en/05_gpu_lod.svg

## Goal
Generated arrays become texture arrays and GPU meshes.

## Key Code Locations
- src/PlanetRenderer.cpp
- include/PlanetRenderer.h

## Explanation Flow
1. setProceduralData() receives CPU data
2. Validate face count and resolution
3. Create height texture array
4. Create water / climate / mask texture arrays
5. Create material weight texture array
6. Upload baked terrain vertex/index buffers
7. Refresh ocean and atmosphere resources if needed
8. Mark renderer data as ready

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
> Hello, on this slide I will talk about **CPU-to-GPU Data Upload**.

> The goal is simple: Generated arrays become texture arrays and GPU meshes.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **setProceduralData() receives CPU data**. This step prepares the next part.

> Second, the step is: **Validate face count and resolution**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Create height texture array**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Create water / climate / mask texture arrays**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Create material weight texture array**. This step prepares the next part.

> Sixth, the step is: **Upload baked terrain vertex/index buffers**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Refresh ocean and atmosphere resources if needed**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Mark renderer data as ready**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> This module connects CPU generation to GPU rendering.

> The generated data is mostly arrays, while shaders prefer texture sampling. The six cube faces are therefore uploaded as layers of texture arrays.

> Height, water, climate, erosion, river and material data can be stored in separate textures or channels.

> Terrain vertices and indices are uploaded into VBOs, IBOs and VAOs.

> After this upload, rendering no longer recalculates the terrain; it samples GPU data and draws meshes.

## Possible Follow-up Answer
Emphasize texture arrays: each cube face is a layer, and shaders sample the correct layer.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.