# 18. Per-Frame Render Sequence

## Matching Images
- ../images_en/11_frame_render_sequence.svg
- ../images_en/30_flow_18_frame_render_sequence.svg

## Goal
A frame is assembled from culling, offscreen passes, terrain, ocean, atmosphere and UI.

## Key Code Locations
- src/PlanetRenderer.cpp
- src/main.cpp

## Explanation Flow
1. Main loop processes input and time
2. Update camera matrices and renderer settings
3. Build visible terrain chunks
4. Build visible ocean patches
5. Render reflection/refraction FBOs
6. Draw main terrain scene
7. Draw ocean, atmosphere and clouds
8. Draw ImGui and swap buffers

## Speaking Script
> This slide ties all modules together by showing what happens in one frame.

> The frame begins with input and time updates, then the camera matrices are refreshed.

> The renderer selects visible terrain chunks and ocean patches based on the camera.

> Ocean reflection and refraction are prepared with offscreen passes before the main scene.

> Then terrain, ocean, atmosphere and clouds are drawn, followed by the ImGui interface.

> This makes the distinction clear: generation prepares data, rendering organizes that data into the final image every frame.

## Possible Follow-up Answer
Use this slide in the middle or near the end to reconnect the individual modules.