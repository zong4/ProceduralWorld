# 05. Hydrology, Erosion and River Masks

## Matching Images
- ../images_en/05_hydrology_erosion_masks.svg
- ../images_en/17_flow_05_hydrology_erosion_masks.svg

## Goal
Downhill flow, stream power and thermal diffusion create channels and erosion traces.

## Key Code Locations
- src/PlanetProceduralData.cpp

## Explanation Flow
1. Sort texels by height
2. Find a downstream receiver for each texel
3. Accumulate drainage / flow
4. Compute streamPower from slope and drainage
5. Carve channels into the terrain
6. Write channel, wear, deposition and flow maps
7. Smooth steep slopes with thermal diffusion
8. Compute water depth, shorelines and moisture effects

## Speaking Script
> This module makes the terrain look shaped by water instead of being pure noise.

> The code sorts texels by height and sends water from higher cells to lower receivers.

> Drainage accumulates downstream. When both drainage and slope are high, stream power becomes strong enough to cut channels.

> The result is written into channel, flow, wear and deposition maps, which can be inspected in debug views.

> Thermal diffusion smooths overly steep slopes, while water depth and shoreline data support coastal and moisture effects.

## Possible Follow-up Answer
A simple way to explain it is: first decide where water flows, then decide how strongly water cuts the terrain.