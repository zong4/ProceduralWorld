# 03. Cube-Sphere Mapping and Seam Handling

## Matching Images
- ../images_en/03_cube_sphere_mapping.svg
- ../images_en/15_flow_03_cube_sphere_mapping.svg

## Goal
The planet is stored as six square faces and mapped onto a sphere.

## Key Code Locations
- src/PlanetProceduralData.cpp
- include/PlanetProceduralData.h

## Explanation Flow
1. Split the planet into six cube faces
2. Each face uses a regular 2D texel grid
3. Convert face UV to cube coordinates
4. Normalize through cubeSphereDirection()
5. Sample noise and climate functions by direction
6. Find cross-face neighbors with neighborCell()
7. Blend borders with fixCubeFaceSeams()
8. Output a continuous spherical height field

## Speaking Script
> Generating a regular grid directly on a sphere is difficult, so this project uses a cube-sphere representation.

> Each face is a normal 2D array. A texel is first positioned on a cube face, then normalized into a direction on the sphere.

> Noise, climate and hydrology calculations use that spherical direction.

> The main challenge is seam handling. Independent faces can produce visible cracks or height discontinuities along borders.

> neighborCell() and fixCubeFaceSeams() blend the borders so the final planet behaves like one continuous surface.

## Possible Follow-up Answer
If asked why not use a UV sphere, answer that cube-sphere sampling is more uniform and works naturally with texture arrays and chunks.