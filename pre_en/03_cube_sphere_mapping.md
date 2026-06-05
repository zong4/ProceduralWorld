# 03. Cube-Sphere Mapping and Seam Handling

## Matching Images
- ../images_en/02_cube_sphere_mapping.svg

## Goal
The planet is stored as six square faces and mapped onto a sphere.

## Key Code Locations
- src/PlanetProceduralData.cpp
- include/PlanetProceduralData.h

## Explanation Flow
1. Split the planet into six cube faces
2. Each face uses a regular 2D texel grid
3. Convert face UV to cube coordinates
4. Normalize through cubeSphereDirection()
5. Sample noise and climate functions by direction
6. Find cross-face neighbors with neighborCell()
7. Blend borders with fixCubeFaceSeams()
8. Output a continuous spherical height field

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
> Hello, on this slide I will talk about **Cube-Sphere Mapping and Seam Handling**.

> The goal is simple: The planet is stored as six square faces and mapped onto a sphere.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **Split the planet into six cube faces**. This step prepares the next part.

> Second, the step is: **Each face uses a regular 2D texel grid**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Convert face UV to cube coordinates**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Normalize through cubeSphereDirection()**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Sample noise and climate functions by direction**. This step prepares the next part.

> Sixth, the step is: **Find cross-face neighbors with neighborCell()**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Blend borders with fixCubeFaceSeams()**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Output a continuous spherical height field**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> Generating a regular grid directly on a sphere is difficult, so this project uses a cube-sphere representation.

> Each face is a normal 2D array. A texel is first positioned on a cube face, then normalized into a direction on the sphere.

> Noise, climate and hydrology calculations use that spherical direction.

> The main challenge is seam handling. Independent faces can produce visible cracks or height discontinuities along borders.

> neighborCell() and fixCubeFaceSeams() blend the borders so the final planet behaves like one continuous surface.

## Possible Follow-up Answer
If asked why not use a UV sphere, answer that cube-sphere sampling is more uniform and works naturally with texture arrays and chunks.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.