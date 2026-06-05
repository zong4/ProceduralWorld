# 06. Climate, Materials and Surface Classification

## Matching Images
- ../images_en/18_flow_06_climate_materials.svg

## Goal
Height, latitude, water and erosion data become renderable surface materials.

## Key Code Locations
- src/PlanetProceduralData.cpp
- shaders/terrain.frag

## Explanation Flow
1. Estimate latitude from sphereDir
2. Reduce temperature by elevation
3. Increase moisture near water, rivers and coasts
4. Use slope to expose rock
5. Use erosion and deposition for color variation
6. Compute materialWeights
7. Upload data as GPU texture arrays
8. Blend grass, rock, snow and coast in terrain shader

## Speaking Script
> The height field defines shape, while this module decides what the surface looks like.

> Temperature depends mainly on latitude and elevation. Moisture depends on water bodies, rivers and coastlines.

> Steep slopes expose more rock. High and cold areas can become snowy. Wet areas near water can look different from dry terrain.

> These values are packed into material weights and uploaded to the GPU.

> The terrain shader blends the final surface colors based on those weights.

## Possible Follow-up Answer
This is a good place to explain that the final look is controlled by more than height: climate, slope and hydrology all matter.