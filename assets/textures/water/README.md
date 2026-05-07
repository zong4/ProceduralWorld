# Water Texture Inputs

Place water material textures in this folder using these exact names.

## Required for the next water-material pass

| File | Channels | Usage |
|------|----------|-------|
| `water_detail_normal_a.png` | RGB normal map | Near-field small ripples, scroll layer A |
| `water_detail_normal_b.png` | RGB normal map | Near-field small ripples, scroll layer B |
| `foam_noise.png` | R or RGB grayscale | Breaks up foam edges only |

## Recommended for higher quality

| File | Channels | Usage |
|------|----------|-------|
| `foam_albedo.png` | RGB | Foam color and texture body |
| `foam_normal.png` | RGB normal map | Foam surface roughness/detail normals |
| `foam_roughness.png` | R grayscale | Higher roughness where foam exists |
| `water_roughness_noise.png` | R grayscale | Subtle water specular breakup |

## Optional later

| File | Channels | Usage |
|------|----------|-------|
| `caustics.png` | R or RGB | Shallow-water caustic projection |
| `bubble_noise.png` | R grayscale | Subsurface bubbles and turbulent water |
| `shore_foam_noise.png` | R grayscale | Separate shoreline foam breakup |

All textures should be tileable. Prefer power-of-two sizes: 512, 1024, or 2048.
