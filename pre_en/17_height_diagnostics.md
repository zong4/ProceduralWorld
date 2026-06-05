# 17. Height Diagnostics Tool

## Matching Images
- ../images_en/29_flow_17_height_diagnostics.svg

## Goal
A standalone tool checks generated height ranges and planet statistics.

## Key Code Locations
- tools/TerrainHeightDiagnostics.cpp
- xmake.lua

## Explanation Flow
1. Run TerrainHeightDiagnostics
2. Create PlanetProceduralSettings
3. Call PlanetProceduralData::generate()
4. Iterate over height data for six faces
5. Compute min, max and average height
6. Check water and land ratios
7. Print diagnostic results
8. Use results to tune generation parameters

## Speaking Script
> The diagnostics tool supports development without opening the full render window.

> It generates procedural data and measures height ranges, average values and water-land ratios.

> This helps detect abnormal settings, such as terrain that is too flat or oceans that cover too much of the planet.

> For presentation, it shows that the project has a validation path beyond visual inspection.

> Future metrics could include river counts, erosion distribution and material coverage.

## Possible Follow-up Answer
Use this as an engineering completeness point: there is a main renderer and a separate diagnostic tool.