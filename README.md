# Procedural World

An OpenGL 4.1 procedural planet renderer built with GLFW, GLAD, GLM, and xmake.

## Features

- Cube-sphere planet rendering with tessellation shaders
- Per-face quadtree LOD on the CPU
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
- `Mouse Wheel`: zoom
- `1`: shaded mode
- `2`: toggle wireframe overlay
- `3`: height-map mode
- `4`: normal mode
- `+` / `-`: increase or decrease tessellation
- `T`: toggle animated terrain time
- `Esc`: quit

## Notes

- Shaders are copied into the build output automatically by `xmake.lua`.
- The current LOD system is a first-pass quadtree implementation. It improves patch density around the camera but does not yet stitch mixed-depth patch borders.

## Progress

**Team size:** 3 | **Target:** 15p | **Current:** 5.5p

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

| Module | Work Planned | Target Points |
|--------|--------------|---------------|
| **Texture splatting** (1p) | Replace procedural biome colors with sampled textures; weight-blend by height + slope (splat map); add per-biome normal maps | 0.5p remaining |
| **Hydraulic erosion** (1–2p) | GPU compute shader particle simulation; iterative sediment carry/deposit modifying the heightmap; produces natural valleys and gullies | 1–2p |
| **Water reflection & refraction** (1–2p) | Planar or screen-space reflection; refraction with depth-based color blend; Fresnel coefficient for reflection intensity | 1p remaining |
| **Procedural sky & sun** (2p) | Preetham or Nishita atmospheric scattering model; time-of-day sun direction | 2p |
| **Scene lighting from sky** (1–3p) | Sun direction drives scene directional light; sky color drives ambient; unified physically-based illumination | 1–3p |
| **Procedural vegetation** (1–∞p) | Instanced billboard grass as baseline; L-system or SDF trees as stretch goal; placement filtered by slope and height | 1p+ |

**Planned subtotal: 6.5–9.5p**
