# 07. Terrain Chunk Baking and Baked LOD

## Matching Images
- ../images_en/05_gpu_lod.svg

## Goal
The DEM is baked into chunks so the renderer only draws visible pieces.

## Key Code Locations
- src/PlanetProceduralData.cpp
- src/PlanetRenderer.cpp
- include/PlanetRenderer.h

## Explanation Flow
1. buildTerrainChunks() divides every face
2. Generate vertices and indices for each chunk
3. Store chunk center, radius and error range
4. buildVisibleBakedChunks() filters by camera
5. Frustum culling removes invisible chunks
6. Distance selects the LOD level
7. Bind the matching VAO / IBO
8. drawBakedTerrainPass() renders terrain

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
> Hello, on this slide I will talk about **Terrain Chunk Baking and Baked LOD**.

> The goal is simple: The DEM is baked into chunks so the renderer only draws visible pieces.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **buildTerrainChunks() divides every face**. This step prepares the next part.

> Second, the step is: **Generate vertices and indices for each chunk**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Store chunk center, radius and error range**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **buildVisibleBakedChunks() filters by camera**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Frustum culling removes invisible chunks**. This step prepares the next part.

> Sixth, the step is: **Distance selects the LOD level**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Bind the matching VAO / IBO**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **drawBakedTerrainPass() renders terrain**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> Drawing the whole planet at full detail would be expensive, so the terrain is baked into chunks.

> Each chunk stores its own vertices, indices, center and bounding information.

> Before rendering, buildVisibleBakedChunks() selects chunks based on the camera, frustum and distance.

> LOD keeps more geometry close to the camera and less geometry far away.

> This is the key performance module for terrain rendering.

## Possible Follow-up Answer
Baked means the mesh is prepared during generation; the render stage mainly selects and submits it.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.