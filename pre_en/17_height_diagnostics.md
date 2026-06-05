# 17. Height Diagnostics Tool

## Matching Images
- ../images_en/08_frame_debug.svg

## Goal
A standalone tool checks generated height ranges and planet statistics.

## Key Code Locations
- tools/TerrainHeightDiagnostics.cpp
- xmake.lua

## Explanation Flow
1. Run TerrainHeightDiagnostics
2. Create PlanetProceduralSettings
3. Call PlanetProceduralData::generate()
4. Iterate over height data for six faces
5. Compute min, max and average height
6. Check water and land ratios
7. Print diagnostic results
8. Use results to tune generation parameters

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
> Hello, on this slide I will talk about **Height Diagnostics Tool**.

> The goal is simple: A standalone tool checks generated height ranges and planet statistics.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **Run TerrainHeightDiagnostics**. This step prepares the next part.

> Second, the step is: **Create PlanetProceduralSettings**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Call PlanetProceduralData::generate()**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Iterate over height data for six faces**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Compute min, max and average height**. This step prepares the next part.

> Sixth, the step is: **Check water and land ratios**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Print diagnostic results**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Use results to tune generation parameters**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> The diagnostics tool supports development without opening the full render window.

> It generates procedural data and measures height ranges, average values and water-land ratios.

> This helps detect abnormal settings, such as terrain that is too flat or oceans that cover too much of the planet.

> For presentation, it shows that the project has a validation path beyond visual inspection.

> Future metrics could include river counts, erosion distribution and material coverage.

## Possible Follow-up Answer
Use this as an engineering completeness point: there is a main renderer and a separate diagnostic tool.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.