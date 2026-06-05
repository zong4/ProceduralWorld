# 11. Ocean Material, Reflection and Refraction

## Matching Images
- ../images_en/06_ocean_fft.svg

## Goal
FBO passes, Fresnel and depth blending produce richer water shading.

## Key Code Locations
- src/PlanetRenderer.cpp
- shaders/ocean.frag

## Explanation Flow
1. drawReflectionRefractionPasses() prepares offscreen images
2. Reflection pass renders reflected sky and terrain
3. Refraction pass renders what is seen through water
4. Main ocean pass samples FFT wave textures
5. Fresnel changes reflection by view angle
6. Water depth blends shallow and deep colors
7. Add highlights, foam, SSS and aerial perspective
8. Output the final ocean color

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
> Hello, on this slide I will talk about **Ocean Material, Reflection and Refraction**.

> The goal is simple: FBO passes, Fresnel and depth blending produce richer water shading.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **drawReflectionRefractionPasses() prepares offscreen images**. This step prepares the next part.

> Second, the step is: **Reflection pass renders reflected sky and terrain**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Refraction pass renders what is seen through water**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Main ocean pass samples FFT wave textures**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Fresnel changes reflection by view angle**. This step prepares the next part.

> Sixth, the step is: **Water depth blends shallow and deep colors**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Add highlights, foam, SSS and aerial perspective**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Output the final ocean color**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> This module explains why the ocean looks richer than a blue surface.

> The renderer first creates reflection and refraction images in offscreen framebuffers.

> The main ocean pass combines those images with FFT wave normals and Fresnel.

> At grazing angles, reflection becomes stronger. Looking down into the water makes refraction and water color more visible.

> Water depth controls the shallow-to-deep color blend, and the shader adds highlights, foam, subsurface scattering and atmospheric distance effects.

## Possible Follow-up Answer
The important point is that ocean shading is multi-pass, not a single flat color.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.