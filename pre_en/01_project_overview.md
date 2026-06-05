# 01. Project Overview

## Matching Images
- ../images_en/01_overall_pipeline.svg
- ../images_en/13_flow_01_project_overview.svg

## Goal
The complete path from application startup and CPU procedural generation to real-time GPU rendering.

## Key Code Locations
- src/main.cpp
- src/PlanetRenderer.cpp
- src/PlanetProceduralData.cpp
- xmake.lua

## Explanation Flow
1. Start main() and create the OpenGL window
2. Initialize ImGui and PlanetRenderer
3. Load shaders, meshes, ocean and atmosphere resources
4. Try to restore session/cache data
5. Without cache, enter the procedural setup screen
6. CPU generates DEM, climate, hydrology and material data
7. Main thread uploads texture arrays and baked meshes
8. Render terrain, ocean, atmosphere, clouds and UI every frame

## Speaking Script
> This slide introduces the main goal: the project is not a static model, but a real-time procedural planet renderer.

> The work is separated into two layers. The CPU generates terrain height, hydrology, erosion, temperature, moisture and material masks. The GPU turns those datasets into the final real-time image.

> The program initializes the window, OpenGL, ImGui and the renderer, then tries to restore local cached data. If no cache is available, the user edits parameters and starts planet generation.

> Once generation is finished, OpenGL resource upload happens on the main thread, because textures and buffers depend on the active OpenGL context.

> The render loop then culls visible regions and draws terrain, ocean, atmosphere, volumetric clouds and debug UI.

## Possible Follow-up Answer
If asked about the core contribution, explain that the project integrates procedural DEM terrain, chunk LOD, FFT ocean, atmosphere scattering and an interactive debug UI into one renderer.