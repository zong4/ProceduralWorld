# 15. Input and Camera Control

## Matching Images
- ../images_en/08_frame_debug.svg

## Goal
Keyboard and mouse input make the planet interactively inspectable.

## Key Code Locations
- src/main.cpp
- include/PlanetRenderer.h

## Explanation Flow
1. GLFW captures keyboard and mouse events
2. main.cpp updates camera state
3. Mouse controls view or orbit
4. Keyboard controls movement, zoom and modes
5. Delta time smooths motion
6. Compute view and projection matrices
7. Pass camera data to PlanetRenderer
8. Shaders use camera position and matrices

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
> Hello, on this slide I will talk about **Input and Camera Control**.

> The goal is simple: Keyboard and mouse input make the planet interactively inspectable.

> If that sounds a little hard, I can say it in easier words: this slide shows what this part does, why it is needed, and what it gives to the next part.

> Please look at the images first. The first image gives the big idea. The flow chart shows the order. I will follow the chart from the first step to the last step.

> In simple words, this module takes some data, does one clear job, and then gives the result to the next part of the project.

> Now I will explain the flow chart slowly.

> First, the step is: **GLFW captures keyboard and mouse events**. This step prepares the next part.

> Second, the step is: **main.cpp updates camera state**. I do not need to explain all code here. I only show the job of this step.

> Third, the step is: **Mouse controls view or orbit**. After this step, the next step has the data or state it needs.

> Fourth, the step is: **Keyboard controls movement, zoom and modes**. I can point to the arrow on the chart and show that the flow moves forward.

> Fifth, the step is: **Delta time smooths motion**. This step prepares the next part.

> Sixth, the step is: **Compute view and projection matrices**. I do not need to explain all code here. I only show the job of this step.

> Seventh, the step is: **Pass camera data to PlanetRenderer**. After this step, the next step has the data or state it needs.

> Eighth, the step is: **Shaders use camera position and matrices**. I can point to the arrow on the chart and show that the flow moves forward.

> To close this slide, I can say this: this module has a clear input, a clear job, and a clear output. That is why it fits into the full planet rendering system.

> If the teacher asks a question, I can look at the answer below. I do not need to add many new words while speaking.

## Optional Detail
> A real-time renderer needs interactive inspection so the viewer can understand both planetary scale and local detail.

> GLFW callbacks and the main loop update the camera state.

> Keyboard and mouse input change position, direction, orbit and zoom values. Delta time keeps motion consistent.

> The final view and projection matrices are passed into PlanetRenderer and then to shaders.

> Camera position also affects terrain LOD, ocean patch LOD, atmospheric view angle and Fresnel.

## Possible Follow-up Answer
The camera is both a viewing tool and an input to rendering decisions.

## Speaking Tips
- Speak slowly and point to the image.
- Use short sentences.
- Do not explain all code lines. Explain the flow first.
- If a word is hard, use the Simple Words section above.