uniform float seaLevelOffset;
uniform vec3 terrainLowlandColor;
uniform vec3 terrainForestColor;
uniform vec3 terrainDesertColor;
uniform vec3 terrainRockColor;
uniform vec3 terrainBeachColor;
uniform vec3 terrainSnowColor;
uniform float terrainBeachWidth;
uniform float terrainRockSlopeStart;
uniform float terrainRockSlopeEnd;
uniform float terrainSnowStart;
uniform float terrainSnowEnd;
uniform float terrainMaterialNoiseScale;
uniform float terrainMaterialNoiseStrength;
uniform sampler2DArray proceduralTemperatureTexture;
uniform sampler2DArray proceduralMoistureTexture;
uniform sampler2DArray proceduralBiomeTexture;

float hash31(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

float valueNoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = hash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash31(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);
    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);
    return mix(nxy0, nxy1, u.z);
}

float fbm3(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float total = 0.0;

    for (int i = 0; i < 4; ++i) {
        value += valueNoise(p) * amplitude;
        total += amplitude;
        p *= 2.03;
        amplitude *= 0.5;
    }

    return value / max(total, 0.0001);
}

SurfaceData sampleSurfaceData(float height, vec3 worldPos, vec3 shadingNormal, vec2 terrainUv, int terrainFaceIndex)
{
    SurfaceData surface;

    vec3 radialUp = normalize(worldPos);
    surface.radialAlignment = clamp(dot(normalize(shadingNormal), radialUp), 0.0, 1.0);
    surface.slope = 1.0 - surface.radialAlignment;
    surface.height01 = height * 0.5 + 0.5;
    vec3 dataUv = vec3(clamp(terrainUv, vec2(0.0), vec2(1.0)), float(terrainFaceIndex));
    float temperature = clamp(texture(proceduralTemperatureTexture, dataUv).r, 0.0, 1.0);
    float moisture = clamp(texture(proceduralMoistureTexture, dataUv).r, 0.0, 1.0);

    float relativeHeight = height - seaLevelOffset;
    float landMask = smoothstep(0.0, max(terrainBeachWidth, 0.0001), relativeHeight);
    float beachMask = (1.0 - smoothstep(terrainBeachWidth * 0.35, terrainBeachWidth, abs(relativeHeight)))
                    * (1.0 - smoothstep(terrainRockSlopeStart * 0.45, terrainRockSlopeStart, surface.slope));
    float rockMask = smoothstep(terrainRockSlopeStart, terrainRockSlopeEnd, surface.slope);
    rockMask = max(rockMask, smoothstep(0.58, 0.82, surface.height01) * 0.45);
    float snowMask = max(
        smoothstep(terrainSnowStart, terrainSnowEnd, surface.height01),
        smoothstep(0.68, 0.86, 1.0 - temperature)
    );
    snowMask *= 1.0 - smoothstep(terrainRockSlopeEnd * 0.85, 1.0, surface.slope) * 0.35;
    float desertWeight = smoothstep(0.55, 0.75, temperature)
                       * (1.0 - smoothstep(0.25, 0.45, moisture))
                       * landMask;
    float forestWeight = smoothstep(0.45, 0.70, moisture)
                       * smoothstep(0.25, 0.45, temperature)
                       * (1.0 - smoothstep(0.80, 1.0, temperature))
                       * landMask;
    float grassWeight = landMask;

    beachMask = clamp(beachMask, 0.0, 1.0);
    rockMask = clamp(rockMask, 0.0, 1.0);
    snowMask = clamp(snowMask, 0.0, 1.0);
    desertWeight = clamp(desertWeight, 0.0, 1.0);
    forestWeight = clamp(forestWeight, 0.0, 1.0);
    grassWeight = clamp(grassWeight, 0.0, 1.0);

    float sumWeights = grassWeight + forestWeight + desertWeight + beachMask + rockMask + snowMask + 0.0001;

    float materialNoise = fbm3(worldPos * terrainMaterialNoiseScale);
    float colorVariation = mix(1.0 - terrainMaterialNoiseStrength, 1.0 + terrainMaterialNoiseStrength, materialNoise);

    vec3 lowlandTint = terrainLowlandColor * mix(0.86, 1.12, fbm3(radialUp * 18.0 + 4.1));
    vec3 color = (
        lowlandTint * grassWeight
      + terrainForestColor * forestWeight
      + terrainDesertColor * desertWeight
      + terrainBeachColor * beachMask
      + terrainRockColor * rockMask
      + terrainSnowColor * snowMask
    ) / sumWeights;
    color = mix(terrainBeachColor * 0.72, color, landMask);

    surface.baseColor = clamp(color * colorVariation, vec3(0.0), vec3(2.0));
    return surface;
}
