# 12. Atmosphere Scattering LUTs

## Matching Images
- ../images_en/24_flow_12_atmosphere_lut.svg

## Goal
Precomputed lookup tables make sky and aerial perspective fast at runtime.

## Key Code Locations
- src/PlanetRenderer.cpp
- shaders/atmosphere*.frag
- shaders/atmosphere*.vert

## Explanation Flow
1. Build a LUT signature from atmosphere parameters
2. Recompute LUTs when parameters change
3. Compute transmittance LUT
4. Compute irradiance LUT
5. Compute scattering LUT
6. atmosphere.frag samples LUTs at runtime
7. Use sun direction, view ray and altitude
8. Composite sky with terrain, ocean and clouds

## Speaking Script
> Atmospheric scattering is expensive if fully integrated per pixel, so the project uses precomputed lookup tables.

> When atmosphere parameters change, the renderer checks a signature and recomputes the LUTs if needed.

> The LUTs store transmittance, irradiance and scattering information.

> At runtime, atmosphere.frag samples these tables to produce sky color, horizon haze and distance fading.

> This trades some preprocessing for much faster frame rendering.

## Possible Follow-up Answer
Explain LUTs as precomputed tables: calculate once, sample many times.