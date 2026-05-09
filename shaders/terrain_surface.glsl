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
uniform float oceanShoreBlendWidth;
uniform float heightScale;
uniform float proceduralDataTexelSize;
uniform sampler2DArray proceduralHeightTexture;
uniform sampler2DArray proceduralTemperatureTexture;
uniform sampler2DArray proceduralMoistureTexture;

#include "planet_sampling.glsl"

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

float runtimeShoreMask(float h)
{
    float signedWaterDepth = (seaLevelOffset - h) * heightScale;
    return 1.0 - smoothstep(0.0, max(oceanShoreBlendWidth, 0.001), abs(signedWaterDepth));
}

struct PlanetSample
{
    float height;
    float bakedHeight;
    float waterDepth;
    float bakedWaterDepth;
    float runtimeWaterDepth;
    float shoreMask;
    float signedHeightFromSea;
    float temperature;
    float moisture;
    vec4 erosionData;
    vec4 biomeA;
    vec4 biomeB;
};

PlanetSample samplePlanet(vec3 sphereDir, float finalHeight)
{
    PlanetSample planetSample;
    planetSample.height = finalHeight;
    planetSample.bakedHeight = sampleFloatArraySeamlessNarrow(proceduralHeightTexture, sphereDir);
    planetSample.bakedWaterDepth = max(sampleFloatArraySeamless(proceduralWaterDepthTexture, sphereDir), 0.0);
    planetSample.runtimeWaterDepth = max((seaLevelOffset - finalHeight) * heightScale, 0.0);
    planetSample.waterDepth = max(planetSample.runtimeWaterDepth, planetSample.bakedWaterDepth * 0.15);
    planetSample.shoreMask = runtimeShoreMask(finalHeight);
    planetSample.signedHeightFromSea = (finalHeight - seaLevelOffset) * heightScale;
    planetSample.temperature = clamp(sampleFloatArraySeamless(proceduralTemperatureTexture, sphereDir), 0.0, 1.0);
    planetSample.moisture = clamp(sampleFloatArraySeamless(proceduralMoistureTexture, sphereDir), 0.0, 1.0);
    planetSample.erosionData = sampleVec4ArraySeamless(proceduralErosionMaskTexture, sphereDir);
    planetSample.biomeA = sampleVec4ArraySeamless(proceduralBiomeWeightATexture, sphereDir);
    planetSample.biomeB = sampleVec4ArraySeamless(proceduralBiomeWeightBTexture, sphereDir);
    return planetSample;
}

SurfaceData sampleSurfaceData(float height, vec3 worldPos, vec3 shadingNormal, vec3 sphereDir)
{
    SurfaceData surface;
    PlanetSample planet = samplePlanet(sphereDir, height);

    vec3 radialUp = normalize(worldPos);
    surface.radialAlignment = clamp(dot(normalize(shadingNormal), radialUp), 0.0, 1.0);
    surface.slope = 1.0 - surface.radialAlignment;
    surface.height01 = height * 0.5 + 0.5;
    float temperature = planet.temperature;
    float moisture = planet.moisture;
    vec4 erosionData = planet.erosionData;
    float channelMask = clamp(erosionData.r, 0.0, 1.0);
    float flowMask = clamp(erosionData.g, 0.0, 1.0);
    float wearMask = clamp(erosionData.b, 0.0, 1.0);
    float depositionMask = clamp(erosionData.a, 0.0, 1.0);
    float waterDepth = planet.waterDepth;
    float signedHeightFromSea = planet.signedHeightFromSea;
    float runtimeShore = planet.shoreMask;
    float runtimeLand = smoothstep(0.0, max(oceanShoreBlendWidth * 0.40, 0.001), signedHeightFromSea);
    float runtimeWater = smoothstep(0.0, max(oceanShoreBlendWidth * 0.45, 0.001), -signedHeightFromSea);

    vec4 biomeA = planet.biomeA;
    vec4 biomeB = planet.biomeB;
    float beachWeight = clamp(biomeA.r, 0.0, 1.0);
    float grassWeight = clamp(biomeA.g, 0.0, 1.0);
    float forestWeight = clamp(biomeA.b, 0.0, 1.0);
    float desertWeight = clamp(biomeA.a, 0.0, 1.0);
    float rockWeight = clamp(biomeB.r, 0.0, 1.0);
    float snowWeight = clamp(biomeB.g, 0.0, 1.0);
    float wetlandWeight = clamp(biomeB.b, 0.0, 1.0);
    float shallowWaterWeight = clamp(biomeB.a, 0.0, 1.0);
    float runtimeBeach = runtimeShore
                       * runtimeLand
                       * (1.0 - smoothstep(terrainRockSlopeStart, terrainRockSlopeEnd, surface.slope));
    float runtimeShallowWater = runtimeWater
                              * (1.0 - smoothstep(max(oceanShoreBlendWidth, 0.001), max(oceanShoreBlendWidth * 4.0, 0.004), waterDepth));
    beachWeight = max(beachWeight, runtimeBeach * 0.65);
    shallowWaterWeight = max(shallowWaterWeight, runtimeShallowWater * 0.75);
    float landWeight = beachWeight + grassWeight + forestWeight + desertWeight + rockWeight + snowWeight + wetlandWeight;
    if (landWeight + shallowWaterWeight <= 0.0001) {
        float relativeHeight = height - seaLevelOffset;
        float landMask = smoothstep(0.0, max(terrainBeachWidth, 0.0001), relativeHeight);
        float seabedMask = smoothstep(0.0001, 0.08, waterDepth);
        float fallbackBeach = (1.0 - smoothstep(terrainBeachWidth * 0.35, terrainBeachWidth, abs(relativeHeight))) * landMask;
        float fallbackRock = smoothstep(terrainRockSlopeStart, terrainRockSlopeEnd, surface.slope) * landMask;
        float fallbackSnow = max(
            smoothstep(terrainSnowStart, terrainSnowEnd, surface.height01),
            smoothstep(0.68, 0.86, 1.0 - temperature)
        ) * landMask;
        float fallbackDesert = smoothstep(0.55, 0.75, temperature)
                             * (1.0 - smoothstep(0.25, 0.45, moisture))
                             * landMask;
        float fallbackForest = smoothstep(0.45, 0.70, moisture)
                             * smoothstep(0.25, 0.45, temperature)
                             * landMask;
        beachWeight = clamp(fallbackBeach, 0.0, 1.0);
        rockWeight = clamp(fallbackRock, 0.0, 1.0);
        snowWeight = clamp(fallbackSnow, 0.0, 1.0);
        desertWeight = clamp(fallbackDesert, 0.0, 1.0);
        forestWeight = clamp(fallbackForest, 0.0, 1.0);
        grassWeight = clamp(landMask, 0.0, 1.0);
        shallowWaterWeight = max(shallowWaterWeight, seabedMask);
        landWeight = beachWeight + grassWeight + forestWeight + desertWeight + rockWeight + snowWeight;
    }

    float materialNoise = fbm3(radialUp * (terrainMaterialNoiseScale * 200.0));
    float colorVariation = mix(1.0 - terrainMaterialNoiseStrength, 1.0 + terrainMaterialNoiseStrength, materialNoise);

    vec3 lowlandTint = terrainLowlandColor * mix(0.86, 1.12, fbm3(radialUp * 18.0 + 4.1));
    vec3 wetlandColor = mix(terrainLowlandColor, terrainForestColor, 0.58) * vec3(0.72, 0.86, 0.76);
    vec3 shallowShelfColor = mix(terrainBeachColor, terrainRockColor, 0.25);
    vec3 sedimentColor = vec3(0.42, 0.36, 0.27);
    vec3 abyssalClayColor = vec3(0.18, 0.16, 0.15);
    vec3 seabedRockColor = terrainRockColor * 0.78;
    vec3 shallowWaterColor = mix(shallowShelfColor, sedimentColor, smoothstep(0.25, 2.5, waterDepth));
    shallowWaterColor = mix(shallowWaterColor, abyssalClayColor, smoothstep(2.0, 10.0, waterDepth));
    shallowWaterColor = mix(shallowWaterColor, seabedRockColor, clamp(wearMask * 0.35 + surface.slope * 0.18, 0.0, 0.55));
    shallowWaterColor = mix(shallowWaterColor, sedimentColor, depositionMask * 0.35);
    vec3 color = (
        terrainBeachColor * beachWeight
      + lowlandTint * grassWeight
      + terrainForestColor * forestWeight
      + terrainDesertColor * desertWeight
      + terrainRockColor * rockWeight
      + terrainSnowColor * snowWeight
      + wetlandColor * wetlandWeight
      + shallowWaterColor * shallowWaterWeight
    ) / max(landWeight + shallowWaterWeight, 0.0001);

    color = mix(color, terrainRockColor, wearMask * 0.35);
    color = mix(color, vec3(0.52, 0.43, 0.30), depositionMask * 0.28);
    color = mix(color, vec3(0.06, 0.16, 0.10), channelMask * 0.12);
    color = mix(color, vec3(0.08, 0.20, 0.11), flowMask * 0.22);
    color = mix(color, terrainBeachColor * 0.72, shallowWaterWeight * 0.20);

    surface.baseColor = clamp(color * colorVariation, vec3(0.0), vec3(2.0));
    return surface;
}
