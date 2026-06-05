# 09. Ocean Patch LOD on a Sphere

## Matching Images
- ../images_en/21_flow_09_ocean_patch_lod.svg

## Goal
Visible ocean patches are selected dynamically and refined with tessellation.

## Key Code Locations
- src/PlanetRenderer.cpp
- shaders/ocean.tesc
- shaders/ocean.tese
- shaders/ocean.frag

## Explanation Flow
1. Create ocean root patches for six cube faces
2. Traverse quadtree with collectVisibleOceanPatches()
3. Cull patches outside the frustum
4. Analyze water coverage per patch
5. Split nodes by distance and error
6. Build the visible ocean patch list
7. Tessellation shader refines patches on the sphere
8. Fragment shader computes water color

## Speaking Script
> The ocean is not a fixed flat plane. It is a set of spherical patches around the planet.

> Each cube face owns root ocean patches, and a quadtree selects visible regions at runtime.

> Patches outside the view or with too little water coverage can be skipped.

> Near patches are subdivided, while distant ones stay coarse.

> The tessellation shader refines the geometry on the sphere, and the fragment shader handles the final water appearance.

## Possible Follow-up Answer
Compare it with terrain LOD: terrain uses baked chunks, while the ocean uses runtime quadtree patches.