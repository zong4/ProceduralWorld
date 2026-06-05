# 04. DEM Terrain Generation

## Matching Images
- ../images_en/04_dem_generation.svg
- ../images_en/16_flow_04_dem_generation.svg

## Goal
The current runtime path enters generateDemPrototype() to produce height, hydrology and surface attributes.

## Key Code Locations
- src/PlanetProceduralData.cpp

## Explanation Flow
1. PlanetProceduralData::generate()
2. Clamp faceResolution
3. Enter generateDemPrototype()
4. Allocate height, water, erosion and climate arrays for six faces
5. Iterate over every texel and compute sphereDir
6. Use fBM / ridgedFbm to form continents and mountains
7. Store height, uplift, landMask and preErosionHeight
8. Run seam blending, hydrology erosion and climate passes
9. Bake terrain chunks with buildTerrainChunks()

## Speaking Script
> This slide should make the real code path clear. PlanetProceduralData::generate() enters generateDemPrototype() and returns, so this is the active generation path.

> DEM means digital elevation model. The code allocates several arrays for each of the six cube faces, including height, water depth, erosion, temperature, moisture, channels and material weights.

> Each texel samples layered noise through its spherical direction. Low-frequency noise forms continents, while mid and high frequencies shape mountains and ridges.

> After the base terrain is generated, the code performs seam correction, hydrology erosion and climate computation.

> Finally buildTerrainChunks() converts the DEM into renderable terrain chunks.

## Possible Follow-up Answer
Do not overstate the retained legacy generation code after the early return; the active path is the DEM prototype path.