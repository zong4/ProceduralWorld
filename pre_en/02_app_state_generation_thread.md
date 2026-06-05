# 02. Application States and Generation Thread

## Matching Images
- ../images_en/02_application_workflow.svg
- ../images_en/14_flow_02_app_state_generation_thread.svg

## Goal
Three stages separate parameter editing, background generation and real-time rendering.

## Key Code Locations
- src/main.cpp

## Explanation Flow
1. ProceduralSetup shows the parameter UI
2. User clicks Generate Planet
3. startPlanetGeneration() copies current settings
4. std::async generates PlanetProceduralData in the background
5. progressCallback updates atomic progress values
6. The main loop keeps refreshing the progress bar
7. When the future is ready, finishPlanetGeneration() runs
8. setProceduralData() uploads GPU data and enters Render

## Speaking Script
> This module shows the engineering structure of the application. Planet generation can take time, so the main thread does not block directly.

> The background thread only computes CPU-side data such as height fields, hydrology and material weights. It does not create OpenGL objects.

> The main loop reads the future status and atomic progress values to keep the UI responsive.

> When generation is done, finishPlanetGeneration() hands the result to PlanetRenderer, and the main thread creates textures, buffers and vertex arrays.

> This design avoids OpenGL threading problems and keeps the program states easy to explain.

## Possible Follow-up Answer
The key point is that threading is used to keep the UI responsive and to keep all OpenGL resource creation on the main thread.