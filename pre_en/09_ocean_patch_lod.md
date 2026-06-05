# 09. Ocean Patch LOD on a Sphere

## Matching Images
- ../images_en/06_ocean_fft.svg

## Goal
Visible ocean patches are selected dynamically and refined with tessellation.

## Key Code Locations
- src/PlanetRenderer.cpp
- shaders/ocean.tesc
- shaders/ocean.tese
- shaders/ocean.frag

## Explanation Flow
1. Create ocean root patches for six cube faces
2. Traverse quadtree with collectVisibleOceanPatches()
3. Cull patches outside the frustum
4. Analyze water coverage per patch
5. Split nodes by distance and error
6. Build the visible ocean patch list
7. Tessellation shader refines patches on the sphere
8. Fragment shader computes water color

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
> Hello, on this slide I will talk about **Ocean Patch LOD on a Sphere**.

> The goal is simple: Visible ocean patches are selected dynamically and refined with tessellation.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **Create ocean root patches for six cube faces**. This step prepares the next part.

> Second, the step is: **Traverse quadtree with collectVisibleOceanPatches()**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Cull patches outside the frustum**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Analyze water coverage per patch**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Split nodes by distance and error**. This step prepares the next part.

> Sixth, the step is: **Build the visible ocean patch list**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Tessellation shader refines patches on the sphere**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Fragment shader computes water color**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> The ocean is not a fixed flat plane. It is a set of spherical patches around the planet.

> Each cube face owns root ocean patches, and a quadtree selects visible regions at runtime.

> Patches outside the view or with too little water coverage can be skipped.

> Near patches are subdivided, while distant ones stay coarse.

> The tessellation shader refines the geometry on the sphere, and the fragment shader handles the final water appearance.

## Possible Follow-up Answer
Compare it with terrain LOD: terrain uses baked chunks, while the ocean uses runtime quadtree patches.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.