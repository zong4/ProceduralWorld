# 20. Terrain, Ocean, Atmosphere, and Clouds - English Speaking Script

## Purpose
This document is a single reading script for the four major graphics modules in the project. It focuses on the procedural and rendering techniques, and it is written in first person so it can be spoken directly in a presentation.

## 1. Terrain

About the terrain module, I build the planet as a six-face **cube-sphere** **digital elevation model**. I chose this layout because it gives me a much more even distribution than a classic **latitude-longitude sphere**, and it avoids the severe stretching that usually appears near the poles.

At the procedural generation stage, I start from **layered noise** instead of a single height map. In `PlanetTerrainGenerator::terrainHeight`, I combine **continent shape noise**, **continent detail noise**, multiple mountain masks, **ridged noise**, valley detail, basin detail, and foothill detail. I also apply **domain warping** several times, because plain stacked noise tends to look repetitive. With warping, the landforms feel more geological, with broad continental masses, sharper mountain chains, and smaller local breakup.

One design choice I made very deliberately is the **orogenic belt** structure. Instead of letting mountains appear everywhere at random, I define several belts with direction, offset, width, and weight. The reason is simple: long mountain provinces read much more like real tectonic regions than isolated random spikes. I then blend those belts with ridged backbone noise, branch noise, crest noise, and regional masks, so the ridges stay connected rather than fragmented.

After the base elevation is built, I do not treat the height field as a single final result. I compute additional procedural layers such as **water depth**, **shore masks**, **erosion masks**, **channel masks**, **flow masks**, **wear masks**, **deposition masks**, **temperature**, **moisture**, **biome weights**, **domain weights**, **mesh density**, and **geometric error**. The reason for separating these channels is that geometry, climate, hydrology, and rendering hints do different jobs. Once they are separated, later passes can reuse the same data instead of recomputing everything from scratch.

I also refine the terrain using the biome data. Some areas are pushed upward and some are pulled downward according to the land bias and depression bias inferred from the biome weights. Then I apply erosion-related passes, river extraction, moisture updates, and biome weighting passes to turn the raw height field into a more believable planetary surface.

For rendering, I bake the terrain into chunks and draw only the visible ones. I rely on **frustum culling**, **horizon culling**, and **screen-space LOD selection**, because a full planet mesh would be far too expensive to render at uniform detail. The chunk system matters because it turns a large procedural height field into a manageable renderable mesh. I also keep a separate debug path for wireframe and feature overlays, which helps me inspect rivers, coastlines, ridges, and erosion behavior without changing the core terrain data.

So the terrain pipeline is really a combination of procedural generation, layered environmental analysis, and **LOD-aware mesh baking**. The result is not just a height map. It is a structured planetary surface that can drive both graphics and simulation.

## 2. Ocean

About the ocean module, I render the water as a spherical tessellated surface that follows the same **cube-sphere** logic as the terrain. I do not draw the whole ocean at one fixed resolution. Instead, I build visible ocean patches with a **quadtree**, then split or keep each patch based on camera distance, projected size, frustum visibility, and horizon visibility. This is the main performance optimization for the ocean surface, because it lets me spend tessellation only where the player can actually see it.

I also add a shoreline-aware rule. If a patch contains a strong shore mask, or if it mixes land and water, I force it toward a higher **LOD**. I chose that because coastlines are where visual error is most noticeable. A smooth open ocean can tolerate coarser geometry, but a shoreline needs finer subdivisions to stay stable and avoid obvious artifacts.

The **tessellation control shader** computes the patch subdivision level from camera distance and patch radius, while also compensating for cube-sphere distortion. That means the ocean does not look evenly sampled in screen space, but instead adapts to the actual spherical layout of the planet. In the **tessellation evaluation shader**, I sample **FFT wave textures** using **triplanar projection**, so the water remains continuous across the sphere without relying on a single flat UV atlas.

For the wave motion itself, I use the **FFT ocean** system to generate **height**, **normal**, and **displacement** textures. I chose a frequency-domain approach because it produces coherent large-scale wave motion efficiently, which is much more practical than trying to simulate every wave directly in the shader. The shader then layers the spectral waves with smaller detail normals. I also compute **choppy horizontal displacement**, so the wave crests do not look too smooth or plastic.

In the fragment shader, I combine several pieces of information at once. I use procedural terrain height and water depth to decide whether a patch is actually covered by water. I use **reflection** and **refraction framebuffers** to sample the surrounding scene. I use **Fresnel** terms, **GGX specular lighting**, and a simplified **subsurface scattering** response to make the surface feel watery rather than metallic. I also blend shallow and deep colors based on the visual water depth, so coastlines stay bright and shallow while deeper regions move toward darker tones.

Another optimization I added is a fast ocean path for distant or simplified rendering. When full ocean material rendering is disabled, the shader can still classify water coverage, estimate depth, and output a simpler surface response. That gives me a cheaper fallback when I need it. In addition, the reflection and refraction buffers are updated with configurable frame strides and distance-based scaling, so the expensive off-screen passes do not have to run at full rate every frame.

I also adapt the tessellation budget to the current patch count. If the visible ocean becomes too dense, I slightly raise the split threshold; if it is light, I let the system recover back toward the normal value. The reason for this adaptive control is to keep frame time more stable when the camera moves quickly or when the visible ocean area changes a lot.

So the ocean module is not only about visual water. It is a full spherical LOD system that combines procedural waves, shoreline classification, dynamic reflections, refractions, and atmosphere-aware shading.

## 3. Atmosphere

About the atmosphere module, I use precomputed **lookup tables** to make sky rendering fast enough for real time. Full atmospheric scattering is expensive if I try to integrate it from scratch for every pixel, so I split the work into LUT generation and runtime sampling. The renderer builds **transmittance**, **irradiance**, and **scattering** tables, and then the final sky pass samples them when the scene is drawn.

I only recompute those LUTs when the atmosphere parameters actually change. I hash the important settings into a signature, and if the signature is the same as the previous frame, I reuse the existing textures. I chose this approach because atmospheric scattering is usually stable from frame to frame, so recomputing it every time would waste a lot of work for almost no visible gain.

In the runtime shader, I render the atmosphere as a full-screen pass. I reconstruct the view ray from the camera, intersect that ray with the atmospheric sphere, and use the scene depth buffer to stop the sky contribution behind visible terrain or ocean geometry. That depth-aware compositing is important, because it keeps the atmospheric haze and horizon glow physically connected to the world instead of drawing a separate fake background.

The shader then samples the irradiance and scattering LUTs using the current view angle, sun angle, and altitude. I blend a low-air color for the upper sky with a stronger horizon color near grazing angles, so the sky transitions naturally from deep blue to bright haze. I also apply simple **tone mapping**, which keeps the result stable when the exposure changes.

What I like about this design is that it stays physically motivated without becoming too expensive. The LUTs capture the expensive part of the light transport, and the final shader only does the screen-space composition and lookup. That gives me a sky that can support terrain, ocean reflections, and clouds without forcing the renderer into a heavy per-pixel scattering solve.

So the atmosphere module is essentially a balance between correctness and speed. I keep the core scattering idea, but I reorganize it into reusable tables so the runtime cost stays low.

## 4. Clouds

About the cloud module, I treat clouds as a **volumetric layer** inside the atmosphere pass rather than as a separate mesh. I chose that design because it keeps the clouds consistent with the sky lighting, the sun direction, and the rest of the planetary shell. It also means I can blend them directly into the atmospheric composition instead of maintaining a second standalone cloud renderer.

I define the clouds as a shell between a bottom radius and a top radius. Inside that shell, I use animated noise to estimate density. The density field is not just one noise function. I combine **fractal Brownian motion**, **domain warping**, and **Worley-style cellular features**, and I modulate them with cloud coverage and cloud sharpness. The reason for that combination is that each technique contributes a different visual behavior: fBm gives broad structure, warping breaks symmetry, and cellular noise creates more organic breakup.

I also move the noise domain with a wind vector, so the cloud layer drifts over time. On top of that, I reshape the vertical profile of the cloud volume so the base, middle, and top of the cloud all behave differently. That helps the clouds look like actual layered volumetric masses rather than a flat noisy slab.

Before **raymarching**, I compute the intervals where the view ray actually intersects the cloud shell. That is an important optimization, because it lets me skip empty regions and only march where the cloud volume exists. During raymarching, I use jittered sample positions to reduce banding, and I stop early when the transmittance becomes too low. For lighting, I estimate cloud shadowing by marching a shorter ray toward the sun and accumulating **optical depth**. That gives me a cheap approximation of self-shadowing and sun attenuation without needing a full volumetric simulation.

I also shape the cloud lighting with several artistic but technically useful terms. I mix ambient scatter, direct light, shadow response, and a **silver-lining** style highlight near the sun-facing edge. The result is that the cloud volume can read softly in the distance but still show stronger structure when the light hits it from the side.

The final cloud color is then composited over the atmosphere and the scene. Because the clouds are rendered in the same atmosphere pass, they naturally inherit the sky color, the sun direction, and the aerial perspective. That makes the whole system feel unified instead of stitched together from separate effects.

So the cloud module is a volumetric, noise-driven, raymarched layer with wind advection, lighting approximation, and shell-based acceleration. It is designed to look rich while still staying practical for real-time rendering.

## Closing

If I summarize the whole graphics pipeline in one sentence, I would say that I generate the planet procedurally on the CPU, convert that data into LOD-aware GPU resources, and then render terrain, ocean, atmosphere, and clouds as one connected planetary system.
