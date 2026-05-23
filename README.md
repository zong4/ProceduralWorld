# Procedural World

An OpenGL 4.1 procedural planet renderer built with GLFW, GLAD, GLM, and xmake.

## Features

- Cube-sphere planet rendering with tessellation shaders
- Per-face quadtree LOD on the CPU
- Erosion-driven primary meandering river rendering from channel/flow masks
- Shaded, wireframe, height-map, and normal visualization modes
- Fly camera controls for inspecting the planet at multiple scales

## Project Structure

```text
include/
  FlyCamera.h        Camera movement and view-matrix helper
  PlanetRenderer.h   Planet rendering API and LOD-facing data structures
  ShaderProgram.h    Shader compilation helper with lightweight #include support

src/
  main.cpp           GLFW application entry point and input wiring
  PlanetRenderer.cpp Planet mesh rendering, cube-sphere mapping, and quadtree LOD

shaders/
  terrain.vert       Node-local UV remapping for the current quadtree patch
  terrain.tesc       Distance-based tessellation control
  terrain.tese       Cube-sphere displacement and normal generation
  terrain.frag       Fragment shader entry point
  terrain_types.glsl Shared terrain fragment data structures
  terrain_surface.glsl Surface color and slope classification
  terrain_lighting.glsl Lighting, fog, and tone mapping helpers
  terrain_debug.glsl Debug visualization helpers
  wire_fine.frag     Fine wireframe overlay color
  wire_coarse.frag   Coarse quadtree grid overlay
```

## Build

1. Install [xmake](https://xmake.io/).
2. Build the project:

```bash
xmake build
```

3. Run the executable:

```bash
xmake run -y
```

## Controls

- `W/A/S/D`: move camera
- `Q/E`: move down/up
- `Right Mouse + Drag`: rotate camera
- `Left Mouse + Drag`: rotate planet manually
- `Mouse Wheel`: zoom
- `1`: shaded mode
- `2`: toggle wireframe overlay
- `3`: height-map mode
- `4`: normal mode
- `Esc`: quit

## Notes

- Shaders are copied into the build output automatically by `xmake.lua`.
- The current LOD system is a first-pass quadtree implementation. It improves patch density around the camera but does not yet stitch mixed-depth patch borders.

## Progress

**Team size:** 3 | **Target:** 15p | **Current estimate:** 11.5p conservative / 12.5p defensible

### Current Score Estimate

The old 5.5p estimate is outdated. The current code now includes CPU procedural planet data, climate/biome fields, hydraulic and thermal erosion, primary meandering river extraction from channel/flow masks, FFT ocean waves, planar reflection/refraction, depth-based water color, Fresnel mixing, and atmospheric scattering.

| Module | Current implementation | Conservative | Defensible |
| --- | --- | ---: | ---: |
| **Noise terrain geometry** | CPU procedural height generation with gradient noise/fBM, continents, mountains, basins, ocean floor, uploaded as GPU texture arrays | 1p | 1p |
| **Variable-resolution tessellation / LOD** | CPU cube-face quadtree LOD, frustum/horizon culling, shore-aware refinement, GPU tessellation control shader | 2p | 2p |
| **Sphere / cube-sphere tessellation** | Six cube faces mapped to a sphere in tessellation evaluation shader; height displaces along sphere normal | 1p | 1p |
| **Height-, slope-, biome-based shading** | Height/slope/biome/erosion-aware procedural material blending with debug masks | 1p | 1p |
| **Basic water + FFT waves** | Spherical ocean pass, FFT height/normal/displacement textures, choppy displacement, detail normals | 2p | 2p |
| **Texture splatting / material blending** | Procedural biome-weight material blending exists, but not full real-texture splatting | 0.5p | 0.5p |
| **Hydraulic + thermal erosion + rivers** | CPU grid hydraulic erosion, flow, sediment capacity, erode/deposit, thermal talus smoothing, erosion masks, primary river extraction, visible river tint/highlight pass | 1p | 1p |
| **Water reflection & refraction** | Planar reflection/refraction FBOs, depth-based color blend, Fresnel mixing, performance controls | 1p | 2p |
| **Procedural sky / atmosphere** | Atmosphere shell with Rayleigh/Mie-style scattering approximation and tunable density/exposure/colors | 2p | 2p |
| **Procedural vegetation** | Not implemented | 0p | 0p |

**Total:** **11.5p conservative**, **12.5p defensible**.

Use the conservative total if the grader is strict about planar reflection/refraction or real texture splatting. Use the defensible total if planar reflection/refraction with depth blend and Fresnel is counted as the full water extension.

Remaining likely points: real sampled terrain texture splatting (+0.5p), more advanced/GPU erosion (+1p), procedural vegetation (+1p to +2p), and a more complete sky system with day/night or god rays (+1p to +3p).

### Completed

| Module | Work Done | Points |
|--------|-----------|--------|
| **Noise terrain geometry** (1p) | GPU-side height generation in TES; 6-octave fBm with lacunarity 2 / gain 0.45; domain-warped input coordinates via two fBm pre-passes; ridge noise blended for sharp peaks; surface normals computed via central finite differences | 1p |
| **Variable-resolution tessellation** (2p) | TCS computes per-edge tessellation level from camera distance to edge midpoint (linear map tessMin→tessMax); TES uses `fractional_even_spacing` for crack-free transitions between adjacent patches | 2p |
| **Sphere tessellation** (1p) | Tessellated sphere geometry as initial rendering target | 1p |
| **Height- & slope-based shading** (1p) | Fragment shader assigns 7 biome colors by normalized height + surface slope (deep ocean, shallow water, sand, grass, forest, bare rock, snow); Blinn-Phong lighting with hemisphere ambient and specular highlight | 0.5p |
| **Basic water surface** (1–2p) | Flat water plane rendered at sea level | 1p |

**Subtotal: 5.5p**

### Planned

#### Texture splatting — up to 1p remaining

| What to build | Expected score |
|---------------|----------------|
| + Sample real textures per biome, blend by splat map | 1p |

#### Hydraulic erosion — 1–2p

| What to build | Expected score |
|---------------|----------------|
| Basic hydraulic erosion: particle simulation on CPU or simple GPU pass, visibly smoother valleys and ridges | 1p |
| + Full GPU compute shader, iterative sediment carry/deposit, realistic gullies and alluvial fans | 2p |

#### Water reflection & refraction — 1p remaining

| What to build | Expected score |
|---------------|----------------|
| + Planar reflection + refraction with depth-based color blend + Fresnel | 2p |

#### Procedural sky & sun — 2–5p total

| What to build | Expected score |
|---------------|----------------|
| Basic sky gradient + hardcoded sun disc | ~1p |
| + Preetham or Nishita atmospheric scattering, physically correct sky color | 2p |
| + Sun direction drives scene directional light color and intensity | 3–4p |
| + Full day/night cycle, sky-ambient coupling, god rays or horizon glow | 5p |

#### Procedural vegetation — 1p+

| What to build | Expected score |
|---------------|----------------|
| Instanced billboard grass, placement filtered by slope and height | 1p |
| + 3D tree meshes (L-system or SDF), wind animation, density variation | 2–3p+ |

<!-- ### Score Summary

| Scenario | Points |
|----------|--------|
| Current | 5.5p |
| + Texture splatting (finish) | +0.5p → **6p** |
| + Erosion (basic) | +1p → **7p** |
| + Water (reflection + refraction) | +1p → **8p** |
| + Sky (Nishita + lighting) | +3p → **11p** |
| + Vegetation (billboards) | +1p → **12p** |
| + Vegetation (trees) + sky stretch | +2–3p → **14–15p** | -->
