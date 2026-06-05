# 13. Procedural Volumetric Clouds

## Matching Images
- ../images_en/07_atmosphere_clouds.svg

## Goal
Noise and raymarching inside the atmosphere shader create adjustable cloud layers.

## Key Code Locations
- shaders/atmosphere.frag
- src/main.cpp

## Explanation Flow
1. UI controls cloud parameters
2. Pass coverage, density, height and thickness
3. Define a cloud layer in atmosphere.frag
4. Raymarch along the view ray
5. Sample layered noise for density
6. Light march approximates illumination and shadowing
7. Blend cloud color with sky scattering
8. Output volumetric-looking clouds

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
> Hello, on this slide I will talk about **Procedural Volumetric Clouds**.

> The goal is simple: Noise and raymarching inside the atmosphere shader create adjustable cloud layers.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **UI controls cloud parameters**. This step prepares the next part.

> Second, the step is: **Pass coverage, density, height and thickness**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Define a cloud layer in atmosphere.frag**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Raymarch along the view ray**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Sample layered noise for density**. This step prepares the next part.

> Sixth, the step is: **Light march approximates illumination and shadowing**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Blend cloud color with sky scattering**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Output volumetric-looking clouds**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> Clouds are a visual module, but they belong naturally with atmosphere because they use sky color and sun direction.

> The UI exposes coverage, sharpness, scale, height, thickness, density and step counts.

> The shader defines a cloud layer in the atmosphere and samples points along the camera ray.

> Noise determines cloud density, and a simplified light march estimates how much sunlight reaches each point.

> The final cloud color is blended with the sky scattering result.

## Possible Follow-up Answer
Mention the quality tradeoff: more raymarch steps give better clouds but cost more performance.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.