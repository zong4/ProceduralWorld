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
- `Esc`: quit

## Notes

- Shaders are copied into the build output automatically by `xmake.lua`.
- The current LOD system is a first-pass quadtree implementation. It improves patch density around the camera but does not yet stitch mixed-depth patch borders.
