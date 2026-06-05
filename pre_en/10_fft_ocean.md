# 10. FFT Ocean Waves

## Matching Images
- ../images_en/06_ocean_fft.svg

## Goal
Frequency-domain wave spectra generate height, normal, displacement and folding textures.

## Key Code Locations
- src/FFTOcean.cpp
- include/FFTOcean.h

## Explanation Flow
1. initialize() sets cascades and resolution
2. buildInitialSpectrum() creates a Phillips spectrum
3. Initialize frequency data from wind, amplitude and gravity
4. update(time) advances spectral phases
5. IFFT converts data to spatial wave fields
6. Compute height, normal, displacement and folding
7. uploadTextures() sends results to GPU
8. Ocean shader samples textures for animated waves

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
> Hello, on this slide I will talk about **FFT Ocean Waves**.

> The goal is simple: Frequency-domain wave spectra generate height, normal, displacement and folding textures.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **initialize() sets cascades and resolution**. This step prepares the next part.

> Second, the step is: **buildInitialSpectrum() creates a Phillips spectrum**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Initialize frequency data from wind, amplitude and gravity**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **update(time) advances spectral phases**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **IFFT converts data to spatial wave fields**. This step prepares the next part.

> Sixth, the step is: **Compute height, normal, displacement and folding**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **uploadTextures() sends results to GPU**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Ocean shader samples textures for animated waves**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> The FFT ocean module makes the water surface move.

> It first builds a wave spectrum in the frequency domain using wind, amplitude and gravity parameters.

> Every frame, the frequency phases advance with time. An inverse FFT converts the result back into spatial wave textures.

> The output includes height, normals, horizontal displacement and folding information.

> The ocean shader samples these textures across multiple cascades to produce continuous waves at different scales.

## Possible Follow-up Answer
You do not need to derive the math; explain that FFT lets complex wave patterns be generated efficiently.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.