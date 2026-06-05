# 11. Ocean Material, Reflection and Refraction

## Matching Images
- ../images_en/09_ocean_reflection_refraction.svg
- ../images_en/23_flow_11_ocean_material.svg

## Goal
FBO passes, Fresnel and depth blending produce richer water shading.

## Key Code Locations
- src/PlanetRenderer.cpp
- shaders/ocean.frag

## Explanation Flow
1. drawReflectionRefractionPasses() prepares offscreen images
2. Reflection pass renders reflected sky and terrain
3. Refraction pass renders what is seen through water
4. Main ocean pass samples FFT wave textures
5. Fresnel changes reflection by view angle
6. Water depth blends shallow and deep colors
7. Add highlights, foam, SSS and aerial perspective
8. Output the final ocean color

## Speaking Script
> This module explains why the ocean looks richer than a blue surface.

> The renderer first creates reflection and refraction images in offscreen framebuffers.

> The main ocean pass combines those images with FFT wave normals and Fresnel.

> At grazing angles, reflection becomes stronger. Looking down into the water makes refraction and water color more visible.

> Water depth controls the shallow-to-deep color blend, and the shader adds highlights, foam, subsurface scattering and atmospheric distance effects.

## Possible Follow-up Answer
The important point is that ocean shading is multi-pass, not a single flat color.