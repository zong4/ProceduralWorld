# 04. DEM Terrain Generation

## Matching Images
- ../images_en/03_dem_terrain.svg

## Goal
The current runtime path enters generateDemPrototype() to produce height, hydrology and surface attributes.

## Key Code Locations
- src/PlanetProceduralData.cpp

## Explanation Flow
1. PlanetProceduralData::generate()
2. Clamp faceResolution
3. Enter generateDemPrototype()
4. Allocate height, water, erosion and climate arrays for six faces
5. Iterate over every texel and compute sphereDir
6. Use fBM / ridgedFbm to form continents and mountains
7. Store height, uplift, landMask and preErosionHeight
8. Run seam blending, hydrology erosion and climate passes
9. Bake terrain chunks with buildTerrainChunks()

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
> Hello, on this slide I will talk about **DEM Terrain Generation**.

> The goal is simple: The current runtime path enters generateDemPrototype() to produce height, hydrology and surface attributes.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **PlanetProceduralData::generate()**. This step prepares the next part.

> Second, the step is: **Clamp faceResolution**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Enter generateDemPrototype()**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Allocate height, water, erosion and climate arrays for six faces**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Iterate over every texel and compute sphereDir**. This step prepares the next part.

> Sixth, the step is: **Use fBM / ridgedFbm to form continents and mountains**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Store height, uplift, landMask and preErosionHeight**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Run seam blending, hydrology erosion and climate passes**. I can point to the arrow on the chart and show that the flow moves forward.

> Ninth, the step is: **Bake terrain chunks with buildTerrainChunks()**. This step prepares the next part.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> This slide should make the real code path clear. PlanetProceduralData::generate() enters generateDemPrototype() and returns, so this is the active generation path.

> DEM means digital elevation model. The code allocates several arrays for each of the six cube faces, including height, water depth, erosion, temperature, moisture, channels and material weights.

> Each texel samples layered noise through its spherical direction. Low-frequency noise forms continents, while mid and high frequencies shape mountains and ridges.

> After the base terrain is generated, the code performs seam correction, hydrology erosion and climate computation.

> Finally buildTerrainChunks() converts the DEM into renderable terrain chunks.

## Possible Follow-up Answer
Do not overstate the retained legacy generation code after the early return; the active path is the DEM prototype path.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.