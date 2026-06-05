# 13. Procedural Volumetric Clouds

## Matching Images
- ../images_en/10_atmosphere_clouds.svg
- ../images_en/25_flow_13_procedural_clouds.svg

## Goal
Noise and raymarching inside the atmosphere shader create adjustable cloud layers.

## Key Code Locations
- shaders/atmosphere.frag
- src/main.cpp

## Explanation Flow
1. UI controls cloud parameters
2. Pass coverage, density, height and thickness
3. Define a cloud layer in atmosphere.frag
4. Raymarch along the view ray
5. Sample layered noise for density
6. Light march approximates illumination and shadowing
7. Blend cloud color with sky scattering
8. Output volumetric-looking clouds

## Speaking Script
> Clouds are a visual module, but they belong naturally with atmosphere because they use sky color and sun direction.

> The UI exposes coverage, sharpness, scale, height, thickness, density and step counts.

> The shader defines a cloud layer in the atmosphere and samples points along the camera ray.

> Noise determines cloud density, and a simplified light march estimates how much sunlight reaches each point.

> The final cloud color is blended with the sky scattering result.

## Possible Follow-up Answer
Mention the quality tradeoff: more raymarch steps give better clouds but cost more performance.