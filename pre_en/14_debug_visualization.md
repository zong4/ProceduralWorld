# 14. Debug Views and Performance UI

## Matching Images
- ../images_en/12_debug_and_presentation.svg
- ../images_en/26_flow_14_debug_visualization.svg

## Goal
ImGui exposes parameters, visualization modes and performance counters.

## Key Code Locations
- src/main.cpp
- src/PlanetRenderer.cpp

## Explanation Flow
1. Render panel exposes terrain, ocean, atmosphere and cloud parameters
2. User switches render modes or debug overlays
3. Renderer updates shader uniforms
4. Hydrology, erosion and material layers can be visualized
5. Performance panel shows frame timing and visible regions
6. Ocean, cloud and baked chunk stats give feedback
7. Feature overlay code exists
8. Initial DEM path and main render do not fully connect overlay yet

## Speaking Script
> This module is useful during a live presentation because it exposes intermediate data, not just the final image.

> The render panel controls terrain, hydrology debug, ocean, atmosphere, clouds and camera quality settings.

> Debug views can show rivers, erosion, material information and LOD behavior.

> The performance panel reports frame time, visible chunks, ocean patches and cloud settings.

> Be accurate about feature overlays: the code exists, but the initial DEM generation path and current main render path do not fully enable it.

## Possible Follow-up Answer
This is a good slide to show engineering honesty: implemented systems, debug support and current limitations.