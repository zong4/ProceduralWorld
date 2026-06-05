# 08. CPU-to-GPU Data Upload

## Matching Images
- ../images_en/06_gpu_data_upload.svg
- ../images_en/20_flow_08_gpu_upload.svg

## Goal
Generated arrays become texture arrays and GPU meshes.

## Key Code Locations
- src/PlanetRenderer.cpp
- include/PlanetRenderer.h

## Explanation Flow
1. setProceduralData() receives CPU data
2. Validate face count and resolution
3. Create height texture array
4. Create water / climate / mask texture arrays
5. Create material weight texture array
6. Upload baked terrain vertex/index buffers
7. Refresh ocean and atmosphere resources if needed
8. Mark renderer data as ready

## Speaking Script
> This module connects CPU generation to GPU rendering.

> The generated data is mostly arrays, while shaders prefer texture sampling. The six cube faces are therefore uploaded as layers of texture arrays.

> Height, water, climate, erosion, river and material data can be stored in separate textures or channels.

> Terrain vertices and indices are uploaded into VBOs, IBOs and VAOs.

> After this upload, rendering no longer recalculates the terrain; it samples GPU data and draws meshes.

## Possible Follow-up Answer
Emphasize texture arrays: each cube face is a layer, and shaders sample the correct layer.