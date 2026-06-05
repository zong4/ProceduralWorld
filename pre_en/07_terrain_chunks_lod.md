# 07. Terrain Chunk Baking and Baked LOD

## Matching Images
- ../images_en/07_baked_chunk_lod.svg
- ../images_en/19_flow_07_terrain_chunks_lod.svg

## Goal
The DEM is baked into chunks so the renderer only draws visible pieces.

## Key Code Locations
- src/PlanetProceduralData.cpp
- src/PlanetRenderer.cpp
- include/PlanetRenderer.h

## Explanation Flow
1. buildTerrainChunks() divides every face
2. Generate vertices and indices for each chunk
3. Store chunk center, radius and error range
4. buildVisibleBakedChunks() filters by camera
5. Frustum culling removes invisible chunks
6. Distance selects the LOD level
7. Bind the matching VAO / IBO
8. drawBakedTerrainPass() renders terrain

## Speaking Script
> Drawing the whole planet at full detail would be expensive, so the terrain is baked into chunks.

> Each chunk stores its own vertices, indices, center and bounding information.

> Before rendering, buildVisibleBakedChunks() selects chunks based on the camera, frustum and distance.

> LOD keeps more geometry close to the camera and less geometry far away.

> This is the key performance module for terrain rendering.

## Possible Follow-up Answer
Baked means the mesh is prepared during generation; the render stage mainly selects and submits it.