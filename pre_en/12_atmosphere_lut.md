# 12. Atmosphere Scattering LUTs

## Matching Images
- ../images_en/07_atmosphere_clouds.svg

## Goal
Precomputed lookup tables make sky and aerial perspective fast at runtime.

## Key Code Locations
- src/PlanetRenderer.cpp
- shaders/atmosphere*.frag
- shaders/atmosphere*.vert

## Explanation Flow
1. Build a LUT signature from atmosphere parameters
2. Recompute LUTs when parameters change
3. Compute transmittance LUT
4. Compute irradiance LUT
5. Compute scattering LUT
6. atmosphere.frag samples LUTs at runtime
7. Use sun direction, view ray and altitude
8. Composite sky with terrain, ocean and clouds

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
> Hello, on this slide I will talk about **Atmosphere Scattering LUTs**.

> The goal is simple: Precomputed lookup tables make sky and aerial perspective fast at runtime.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **Build a LUT signature from atmosphere parameters**. This step prepares the next part.

> Second, the step is: **Recompute LUTs when parameters change**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Compute transmittance LUT**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Compute irradiance LUT**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Compute scattering LUT**. This step prepares the next part.

> Sixth, the step is: **atmosphere.frag samples LUTs at runtime**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Use sun direction, view ray and altitude**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Composite sky with terrain, ocean and clouds**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> Atmospheric scattering is expensive if fully integrated per pixel, so the project uses precomputed lookup tables.

> When atmosphere parameters change, the renderer checks a signature and recomputes the LUTs if needed.

> The LUTs store transmittance, irradiance and scattering information.

> At runtime, atmosphere.frag samples these tables to produce sky color, horizon haze and distance fading.

> This trades some preprocessing for much faster frame rendering.

## Possible Follow-up Answer
Explain LUTs as precomputed tables: calculate once, sample many times.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.