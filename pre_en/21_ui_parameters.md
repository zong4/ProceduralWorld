# UI Parameter Guide

This file explains the main UI-controllable parameters in English. I group them into two parts: planet generation and rendering. Some settings are shared by both systems, but I list them where they have the strongest effect.

## Planet Generation

| Parameter | Meaning and Impact |
| --- | --- |
| `planetRadius` | Sets the base planet size. A larger value makes the whole world physically bigger and also changes the camera scale and terrain placement. |
| `seaLevelOffset` | Shifts the sea level relative to the terrain height field. Raising it creates more ocean coverage, while lowering it exposes more land. |
| `terrainHeightScale` | Controls how strongly height values are converted into world-space elevation. A larger scale produces taller mountains and deeper valleys. |
| `terrainNoiseScale` | Controls the frequency of the procedural noise used for continents, mountains, and local breakup. Higher values make the terrain more detailed and more crowded. |
| `mountainMaskStrength` | Boosts the influence of mountain provinces. Increasing it makes mountain belts stronger and more visible. |
| `mountainMaskScale` | Controls the spatial size of the mountain mask system. Larger values spread mountain structures over wider regions. |
| `mountainRidgeSharpness` | Controls how sharp and ridge-like the mountain noise becomes. Higher values create narrower peaks and stronger crests. |
| `erosionIterations` | Sets how many erosion passes are applied during generation. More iterations produce more carved rivers and smoother hillslopes, but take longer to generate. |
| `erosionStrength` | Controls the overall intensity of hydraulic erosion. Higher values carve deeper channels and remove more material from slopes. |
| `erosionTalus` | Sets the slope threshold for sediment movement and slope stability. Higher values make terrain tolerate steeper slopes before material starts to slide. |
| `erosionSediment` | Controls how much eroded material is carried and deposited. Higher values increase sediment transport and soft deposition effects. |
| `erosionThermalStrength` | Controls thermal erosion, which smooths steep slopes by diffusing material downhill. Higher values make hillslopes softer and less jagged. |
| `runtimeMountainScale` | Adjusts the visual height of mountain features at render time. It does not rebuild the world, but it changes how strong mountains look in the final image. |

## Rendering

| Parameter | Meaning and Impact |
| --- | --- |
| `renderTerrain` | Toggles terrain rendering on or off. Turning it off hides all land geometry and related terrain shading. |
| `renderOcean` | Toggles ocean rendering on or off. Turning it off removes the water surface completely. |
| `renderAtmosphere` | Toggles atmosphere rendering on or off. Turning it off removes sky scattering and aerial perspective. |
| `renderClouds` | Toggles volumetric clouds on or off. Turning it off leaves only the sky and atmosphere. |
| `renderMode` | Selects the terrain shading mode, such as shaded, unshaded, height, normals, or material view. It is mainly a debugging and inspection control. |
| `wireMode` | Selects wire or diagnostic overlays. It is useful for inspecting ocean patches, baked terrain LOD, or mountain masks. |
| `featureOverlayMode` | Chooses which procedural terrain features are drawn as overlays, such as rivers, coasts, ridges, or erosion edges. |
| `skyColor` | Sets the clear color of the sky and the background tone used in off-screen passes. It strongly affects the overall atmosphere of the scene. |
| `fogDensity` | Controls distance fog on terrain. Higher values fade far terrain into the sky color more aggressively. |

### Terrain Appearance

| Parameter | Meaning and Impact |
| --- | --- |
| `terrainLowlandColor` | Base color used for lowland terrain shading. It affects grassland and plain regions. |
| `terrainForestColor` | Base color used for forested terrain. It pushes woodland areas toward darker green tones. |
| `terrainDesertColor` | Base color used for dry terrain. It controls the look of arid regions and exposed soil. |
| `terrainRockColor` | Base color used for rocky terrain. It shapes cliffs, slopes, and exposed stone. |
| `terrainBeachColor` | Base color used for beaches and shoreline sand. It affects the transition between land and sea. |
| `terrainSnowColor` | Base color used for snowy terrain. It controls the appearance of high or cold regions. |
| `terrainPaletteLowGrass` | Fine-tuning color for low grass regions. It adds variation inside green lowland terrain. |
| `terrainPaletteMeadow` | Fine-tuning color for meadow regions. It makes open grassy areas brighter and livelier. |
| `terrainPaletteForestDark` | Dark forest accent color. It helps create depth in dense vegetation. |
| `terrainPaletteForestWarm` | Warm forest accent color. It adds variation to forested terrain. |
| `terrainPaletteSavanna` | Color for dry grassland and savanna-like terrain. It gives warm open plains a distinct tint. |
| `terrainPaletteDrySoil` | Color for dry soil. It supports barren and drought-affected areas. |
| `terrainPaletteOchre` | Warm earthy accent color. It is useful for exposed sediment and dry slopes. |
| `terrainPaletteWetGreen` | Color for wet vegetation zones. It makes moist land feel richer and darker. |
| `terrainPaletteTundra` | Color for cold, sparse vegetation. It helps high-latitude or high-altitude regions look colder. |
| `terrainPaletteBrownSlope` | Color for brown slopes and transitional hillsides. It softens the shift between green land and rock. |
| `terrainPaletteRedSoil` | Color for red soil and iron-rich terrain. It adds visual variety to dry or weathered ground. |
| `terrainPaletteRockWarm` | Warm rock color used on exposed cliffs and mountain faces. |
| `terrainPaletteRockCool` | Cooler rock color used to balance warm stone tones. |
| `terrainPaletteRockDark` | Dark rock color for shadowed or rough stone surfaces. |
| `terrainPalettePaleStone` | Light stone color for chalky or pale rock areas. |
| `terrainPaletteSnow` | Snow highlight color. It makes snow-capped regions brighter and cleaner. |
| `terrainPaletteSnowShadow` | Shadowed snow color. It keeps snowy terrain from looking flat. |
| `terrainPaletteBeach` | Fine-tuning color for beaches. It improves shoreline sand transitions. |
| `terrainPaletteRiverBed` | Color used for river beds and dry channels. It makes carved river paths readable. |
| `terrainPaletteShallowSeabed` | Color used for shallow seabed areas. It helps underwater coastal zones stay visible. |
| `terrainPaletteDeepSeabed` | Color used for deep seabed areas. It gives deep underwater regions a darker tone. |
| `terrainBeachWidth` | Controls how wide the beach transition zone is. A larger value makes shorelines broader and softer. |
| `terrainRockSlopeStart` | Sets the slope value where terrain starts turning into rock. Lower values make rock appear earlier on gentler slopes. |
| `terrainRockSlopeEnd` | Sets the slope value where rock becomes fully dominant. Higher values keep mixed materials visible on steeper ground. |
| `terrainSnowStart` | Sets the height or exposure range where snow begins to appear. Lower values let snow appear sooner. |
| `terrainSnowEnd` | Sets the range where snow coverage becomes complete. Higher values delay full snow coverage. |
| `terrainMaterialNoiseScale` | Controls the frequency of small-scale material variation noise. Higher values create finer surface breakup. |
| `terrainMaterialNoiseStrength` | Controls how strongly material noise affects color variation. Higher values make the surface look less uniform. |

### Ocean Appearance and Water Behavior

| Parameter | Meaning and Impact |
| --- | --- |
| `oceanAlpha` | Overall opacity of the ocean surface. Lower values make water more transparent. |
| `oceanShallowAlpha` | Opacity used for shallow water. It helps shore water feel lighter and thinner. |
| `oceanDeepAlpha` | Opacity used for deep water. It makes open ocean feel denser and less transparent. |
| `oceanFresnelStrength` | Controls the strength of the Fresnel reflection effect. Higher values make glancing angles reflect more strongly. |
| `oceanDistortionStrength` | Controls screen-space distortion from the water surface. Higher values create stronger refraction and waviness. |
| `oceanDepthRange` | Sets the depth interval used for depth-based color blending. Larger values spread the shallow-to-deep transition over a wider range. |
| `oceanShallowDepthRange` | Sets the range used to classify shallow water. It mainly affects shoreline color blending. |
| `oceanDepthScale` | Scales the visual influence of water depth. Higher values make the difference between shallow and deep water more obvious. |
| `oceanTintStrength` | Controls how strongly the base water tint affects the final color. Higher values make the water color more pronounced. |
| `renderOceanWaves` | Toggles wave displacement and wave normals on or off. Turning it off makes the water much smoother. |
| `renderOceanMaterial` | Toggles the full ocean material response on or off. Turning it off uses a simpler fallback style. |
| `oceanWaveAmplitude` | Sets the height of FFT-driven waves. Higher values create larger swells. |
| `oceanChoppiness` | Controls horizontal wave displacement. Higher values make wave crests sharper and less round. |
| `oceanWaveTileScale` | Controls how often wave textures repeat across the sphere. Higher values make the wave pattern denser. |
| `oceanWaveNormalStrength` | Controls the influence of FFT wave normals. Higher values make the large wave shape more visible. |
| `oceanDetailNormalStrength` | Controls the strength of small-scale detail normals. Higher values add fine surface texture. |
| `oceanDetailNormalScale` | Controls the frequency of the detail normal textures. Higher values make the micro surface pattern tighter. |
| `oceanDetailFadeDistance` | Sets the distance where detail normals fade out. Larger values keep detail visible farther away. |
| `oceanSpecularStrength` | Controls the strength of water highlights. Higher values produce brighter sun glints. |
| `oceanSpecularSharpness` | Controls how tight the specular highlight is. Higher values create sharper reflections. |
| `oceanRoughness` | Controls the base roughness of the water surface. Higher values make the water look less polished. |
| `oceanSSSStrength` | Controls subsurface-scattering-like light in the water. Higher values make shallow water feel softer and brighter. |
| `oceanSSSPower` | Shapes how quickly the subsurface effect falls off with angle. It changes the softness of the water body. |
| `oceanShoreBlendWidth` | Controls the width of the shoreline blending zone. Larger values make the transition between land and water smoother. |
| `renderOceanReflectionRefraction` | Toggles the reflection and refraction capture passes. Turning it off removes those expensive off-screen buffers. |
| `renderOceanReflection` | Toggles the reflection buffer specifically. |
| `renderOceanRefraction` | Toggles the refraction buffer specifically. |
| `oceanReflectionResolutionScale` | Scales the resolution of the reflection and refraction render targets. Lower values improve performance at the cost of detail. |
| `oceanReflectionFrameStride` | Controls how often the reflection buffer is refreshed. Higher values reduce cost but update less often. |
| `oceanRefractionFrameStride` | Controls how often the refraction buffer is refreshed. Higher values reduce cost but update less often. |
| `oceanAutoDistanceLod` | Enables distance-based quality reduction for reflection and refraction. This helps keep performance stable when the camera moves away. |
| `oceanReflectionMaxAltitude` | Sets the altitude limit for reflection quality fading. Above this height, reflections are reduced more aggressively. |
| `oceanRefractionMaxAltitude` | Sets the altitude limit for refraction quality fading. Above this height, refraction becomes less important. |
| `oceanTessellationMax` | Sets the maximum tessellation level for ocean patches. Higher values create denser ocean geometry near the camera. |
| `oceanTessellationMin` | Sets the minimum tessellation level for ocean patches. Lower values make distant ocean more coarse. |
| `oceanTessellationNearDistance` | Sets the distance where tessellation starts to stay high. Smaller values keep detail close to the camera. |
| `oceanTessellationFarDistance` | Sets the distance where tessellation begins to fall back. Larger values keep detail over a wider range. |
| `oceanFftCascadeCount` | Sets how many FFT wave cascades are used. More cascades add more wave scales, but cost more update time. |
| `oceanFftFrameStride` | Controls how often the FFT ocean simulation updates. Higher values reduce cost but update waves less frequently. |
| `oceanShallowColor` | Base color for shallow water. It controls the near-shore water tint. |
| `oceanDeepColor` | Base color for deep water. It controls the open-ocean tint. |
| `oceanSSSColor` | Color used for the subsurface-scattering-like water glow. It affects shallow wave edges and lit water volumes. |

### Atmosphere and Clouds

| Parameter | Meaning and Impact |
| --- | --- |
| `atmosphereHeight` | Sets the thickness of the atmospheric shell. Larger values make the planet look more wrapped in air. |
| `atmosphereDensity` | Controls overall atmosphere opacity and scattering strength. Higher values make haze and sky effects stronger. |
| `atmosphereRayleighStrength` | Controls Rayleigh scattering strength. Higher values intensify the blue sky response. |
| `atmosphereMieStrength` | Controls Mie scattering strength. Higher values make haze, glare, and forward scattering stronger. |
| `atmosphereMieAnisotropy` | Controls how strongly light is scattered in the forward direction. Higher values make the sun glow and haze more directional. |
| `atmosphereExposure` | Controls the final exposure used when tone mapping the sky. Higher values make the atmosphere brighter. |
| `atmosphereRayleighColor` | Color tint used for Rayleigh scattering. It shapes the main sky color. |
| `atmosphereMieColor` | Color tint used for Mie scattering. It shapes haze and warm light response. |
| `cloudCoverage` | Controls how much of the sky is filled with clouds. Higher values create denser cloud fields. |
| `cloudSharpness` | Controls how hard or soft cloud edges are. Higher values create more defined, sharper cloud shapes. |
| `cloudScale` | Controls the size of cloud patterns in the noise field. Larger values produce bigger cloud structures. |
| `cloudSpeed` | Controls cloud movement speed. Higher values make the cloud layer drift faster. |
| `cloudHeight` | Sets the cloud layer altitude above the planet. Higher values lift the cloud deck upward. |
| `cloudThickness` | Controls the vertical thickness of the cloud shell. Larger values create a taller volumetric cloud layer. |
| `cloudDensity` | Controls how dense the cloud volume is. Higher values make clouds darker and more opaque. |
| `cloudShadowStrength` | Controls cloud self-shadowing strength. Higher values make clouds cast deeper internal shadows. |
| `cloudOpacity` | Controls the alpha of the cloud volume. Higher values make clouds block more background light. |
| `cloudStepCount` | Sets the number of raymarch steps used for cloud rendering. More steps improve quality but cost more performance. |
| `cloudLightStepCount` | Sets the number of steps used for cloud light sampling. More steps improve shadow quality but increase cost. |
| `cloudColor` | Base color of the cloud layer. It controls the general cloud tint under lighting. |

### Debug and Camera Constraints

| Parameter | Meaning and Impact |
| --- | --- |
| `cameraNearPlane` | Near clipping plane used for depth reconstruction and water/atmosphere compositing. |
| `cameraFarPlane` | Far clipping plane used for depth reconstruction and scene depth linearization. |
| `coarseGridLineWidth` | Width of the coarse ocean grid or debug line overlay. It affects how visible the diagnostic lines are. |

## Short Summary

If I summarize the whole UI in one sentence, the generation controls shape the planet's structure and climate, while the rendering controls shape the final visual style, water behavior, atmosphere, clouds, and debug overlays.
