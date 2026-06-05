# 15. Input and Camera Control

## Matching Images
- ../images_en/27_flow_15_input_camera.svg

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

## Speaking Script
> A real-time renderer needs interactive inspection so the viewer can understand both planetary scale and local detail.

> GLFW callbacks and the main loop update the camera state.

> Keyboard and mouse input change position, direction, orbit and zoom values. Delta time keeps motion consistent.

> The final view and projection matrices are passed into PlanetRenderer and then to shaders.

> Camera position also affects terrain LOD, ocean patch LOD, atmospheric view angle and Fresnel.

## Possible Follow-up Answer
The camera is both a viewing tool and an input to rendering decisions.