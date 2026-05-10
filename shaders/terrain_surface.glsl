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

float sharpenBiomeWeight(float weight, float exponent)
{
    return pow(clamp(weight, 0.0, 1.0), exponent);
}

float coastalShelter(vec3 sphereDir)
{
    vec3 p = sphereDir * 3.7;
    float broad = fbm3(p + vec3(12.3, 4.7, 8.1));
    float pocket = fbm3(p * 2.35 + vec3(5.7, 17.9, 2.8));
    float notch = 1.0 - abs(fbm3(p * 5.2 + vec3(31.4, 7.6, 19.3)) * 2.0 - 1.0);
    float sheltered = broad * 0.50 + pocket * 0.30 + notch * 0.20;
    return smoothstep(0.42, 0.78, sheltered);
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
    float relativeHeight = height - seaLevelOffset;
    float runtimeShore = planet.shoreMask;
    float runtimeLand = smoothstep(0.0, max(oceanShoreBlendWidth * 0.40, 0.001), signedHeightFromSea);
    float runtimeWater = smoothstep(0.0, max(oceanShoreBlendWidth * 0.45, 0.001), -signedHeightFromSea);
    float coastShelter = coastalShelter(sphereDir);
    float coastExposure = 1.0 - coastShelter;

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
                       * coastShelter
                       * (1.0 - smoothstep(terrainRockSlopeStart, terrainRockSlopeEnd, surface.slope));
    float beachPocket = smoothstep(
        0.58,
        0.78,
        fbm3(radialUp * 46.0 + vec3(17.2, 5.8, 39.4)) * 0.64 + coastShelter * 0.22 + beachWeight * 0.22
    );
    float beachShelf = (1.0 - smoothstep(terrainBeachWidth * 0.65, terrainBeachWidth * 2.35, relativeHeight))
                     * smoothstep(0.0, max(terrainBeachWidth * 0.16, 0.0001), relativeHeight)
                     * coastShelter
                     * beachPocket
                     * (1.0 - smoothstep(terrainRockSlopeStart * 0.75, terrainRockSlopeEnd, surface.slope))
                     * (1.0 - clamp(wearMask * 0.55, 0.0, 0.65));
    float authoredBeach = beachWeight;
    runtimeBeach = max(runtimeBeach * beachPocket * 0.58, beachShelf) * smoothstep(0.025, 0.18, authoredBeach);
    float runtimeRockCoast = runtimeShore
                           * runtimeLand
                           * coastExposure
                           * smoothstep(terrainRockSlopeStart * 0.42, terrainRockSlopeEnd, surface.slope + wearMask * 0.24);
    float runtimeShallowWater = runtimeWater
                              * (1.0 - smoothstep(max(oceanShoreBlendWidth, 0.001), max(oceanShoreBlendWidth * 4.0, 0.004), waterDepth));
    beachWeight = max(beachWeight, runtimeBeach * 0.42);
    rockWeight = max(rockWeight, runtimeRockCoast * 0.78);
    shallowWaterWeight = max(shallowWaterWeight, runtimeShallowWater * 0.75);

    float beachDominance = clamp(beachWeight * 0.72, 0.0, 0.85);
    grassWeight *= 1.0 - beachDominance;
    forestWeight *= 1.0 - beachDominance;
    desertWeight *= 1.0 - beachDominance * 0.85;
    wetlandWeight *= 1.0 - beachDominance * 0.60;
    rockWeight *= 1.0 - beachDominance * 0.35;

    float mountainDesertCull = smoothstep(0.115, 0.245, relativeHeight + surface.slope * 0.46 + wearMask * 0.20);
    float desertPlain = (1.0 - smoothstep(0.12, 0.34, surface.slope))
                      * (1.0 - smoothstep(0.14, 0.32, relativeHeight))
                      * (1.0 - mountainDesertCull)
                      * (1.0 - clamp(channelMask * 0.70 + flowMask * 0.55 + wearMask * 0.35, 0.0, 0.85));
    float displacedDesert = desertWeight * (1.0 - desertPlain);
    desertWeight *= mix(0.20, 1.0, desertPlain);
    rockWeight += displacedDesert * smoothstep(0.18, 0.46, surface.slope + relativeHeight * 0.20) * 0.72;
    grassWeight += displacedDesert * (1.0 - smoothstep(0.18, 0.46, surface.slope + relativeHeight * 0.20)) * 0.34;

    float alpineVegetationCull = smoothstep(0.105, 0.235, relativeHeight + surface.slope * 0.32);
    float forestSlopeViability = 1.0 - smoothstep(0.14, 0.32, surface.slope);
    forestSlopeViability *= 1.0 - smoothstep(0.095, 0.205, relativeHeight);
    forestSlopeViability *= 1.0 - smoothstep(0.115, 0.255, relativeHeight + surface.slope * 0.70);
    forestSlopeViability *= 1.0 - alpineVegetationCull * 0.96;
    forestSlopeViability *= 1.0 - clamp(wearMask * 0.28 + channelMask * 0.16, 0.0, 0.52);
    float displacedForest = forestWeight * (1.0 - forestSlopeViability);
    forestWeight *= mix(0.05, 1.0, forestSlopeViability);
    rockWeight += displacedForest * smoothstep(0.10, 0.32, surface.slope + wearMask * 0.18) * 0.72;
    grassWeight += displacedForest * (1.0 - smoothstep(0.10, 0.32, surface.slope + wearMask * 0.18)) * 0.30;

    float highlandForestCull = smoothstep(0.115, 0.255, relativeHeight + surface.slope * 0.55);
    float culledForest = forestWeight * highlandForestCull;
    forestWeight -= culledForest * 0.85;
    rockWeight += culledForest * 0.65;
    grassWeight += culledForest * (1.0 - smoothstep(0.12, 0.30, surface.slope)) * (1.0 - alpineVegetationCull) * 0.10;

    float grassPlainViability = (1.0 - smoothstep(0.10, 0.28, surface.slope))
                              * (1.0 - smoothstep(0.105, 0.235, relativeHeight + surface.slope * 0.24));
    float displacedGrass = grassWeight * (1.0 - grassPlainViability);
    grassWeight *= mix(0.18, 1.0, grassPlainViability);
    rockWeight += displacedGrass * smoothstep(0.18, 0.42, surface.slope + relativeHeight * 0.15) * 0.52;

    float alpineMaterialMask = smoothstep(0.14, 0.38, relativeHeight + surface.slope * 0.42);
    float steepRockMask = smoothstep(0.20, 0.50, surface.slope);
    float dryRockPlain = smoothstep(0.04, 0.20, relativeHeight)
                       * (1.0 - smoothstep(0.38, 0.76, relativeHeight))
                       * (1.0 - smoothstep(0.22, 0.48, surface.slope))
                       * (1.0 - smoothstep(0.34, 0.62, moisture))
                       * smoothstep(0.52, 0.74, fbm3(radialUp * 9.4 + vec3(21.3, 6.7, 42.8)));
    float ridgeRockMask = smoothstep(0.08, 0.30, relativeHeight)
                        * smoothstep(0.16, 0.38, surface.slope);
    float erodedOutcropMask = wearMask
                            * smoothstep(0.12, 0.34, surface.slope + relativeHeight * 0.22);
    float exposedRockMask = clamp(
        max(steepRockMask * (0.42 + alpineMaterialMask * 0.58), ridgeRockMask)
      + erodedOutcropMask * 0.58,
        0.0,
        1.0
    );
    exposedRockMask *= runtimeLand * (1.0 - beachWeight * 0.58) * (1.0 - wetlandWeight * 0.45);
    rockWeight *= mix(0.76, 1.0, clamp(alpineMaterialMask + steepRockMask * 0.55, 0.0, 1.0));
    rockWeight = max(rockWeight, exposedRockMask * 0.82);
    rockWeight = max(rockWeight, dryRockPlain * 0.68);
    rockWeight += wearMask * steepRockMask * max(alpineMaterialMask, ridgeRockMask) * 0.48;
    grassWeight *= 1.0 - exposedRockMask * 0.42;
    forestWeight *= 1.0 - exposedRockMask * 0.56;
    desertWeight *= 1.0 - exposedRockMask * 0.30;
    grassWeight *= 1.0 - dryRockPlain * 0.48;
    forestWeight *= 1.0 - dryRockPlain * 0.62;
    desertWeight *= 1.0 - dryRockPlain * 0.28;
    grassWeight *= 1.0 - alpineVegetationCull * 0.94;
    forestWeight *= 1.0 - alpineVegetationCull * 0.98;
    snowWeight *= smoothstep(0.28, 0.56, relativeHeight)
                * smoothstep(0.42, 0.72, 1.0 - temperature);
    snowWeight += alpineMaterialMask
                * smoothstep(0.36, 0.66, 1.0 - temperature)
                * smoothstep(0.28, 0.58, relativeHeight)
                * 0.20;

    float biomeBreakup = fbm3(radialUp * 32.0 + vec3(11.7, 4.3, 19.2));
    grassWeight *= mix(0.88, 1.10, fbm3(radialUp * 24.0 + vec3(2.1, 8.4, 5.7)));
    forestWeight *= mix(0.82, 1.16, biomeBreakup);
    desertWeight *= mix(0.84, 1.18, fbm3(radialUp * 18.0 + vec3(31.2, 6.6, 14.8)));
    wetlandWeight *= mix(0.76, 1.20, fbm3(radialUp * 38.0 + vec3(5.6, 22.4, 7.1)));

    float dominantLandWeight = max(max(max(beachWeight, grassWeight), max(forestWeight, desertWeight)), max(max(rockWeight, snowWeight), wetlandWeight));
    float biomeContrast = mix(1.32, 2.32, smoothstep(0.16, 0.52, dominantLandWeight));
    beachWeight = sharpenBiomeWeight(beachWeight, biomeContrast * 0.88);
    grassWeight = sharpenBiomeWeight(grassWeight, biomeContrast);
    forestWeight = sharpenBiomeWeight(forestWeight, biomeContrast * 0.92);
    desertWeight = sharpenBiomeWeight(desertWeight, biomeContrast * 1.08);
    rockWeight = sharpenBiomeWeight(rockWeight, biomeContrast * 0.95);
    snowWeight = sharpenBiomeWeight(snowWeight, biomeContrast * 0.78);
    wetlandWeight = sharpenBiomeWeight(wetlandWeight, biomeContrast * 0.92);

    float treeLineBlend = smoothstep(0.02, 0.22, snowWeight) * smoothstep(0.02, 0.28, forestWeight);
    float snowFeather = snowWeight * treeLineBlend * 0.38;
    snowWeight -= snowFeather;
    forestWeight -= forestWeight * treeLineBlend * 0.22;
    grassWeight += snowFeather * 0.34;
    rockWeight += snowFeather * 0.28;

    float landWeight = beachWeight + grassWeight + forestWeight + desertWeight + rockWeight + snowWeight + wetlandWeight;
    if (landWeight + shallowWaterWeight <= 0.0001) {
        float landMask = smoothstep(0.0, max(terrainBeachWidth, 0.0001), relativeHeight);
        float seabedMask = smoothstep(0.0001, 0.08, waterDepth);
        float fallbackBeach = (1.0 - smoothstep(terrainBeachWidth * 0.35, terrainBeachWidth, abs(relativeHeight)))
                            * landMask
                            * coastShelter;
        float fallbackRock = max(
            smoothstep(terrainRockSlopeStart, terrainRockSlopeEnd, surface.slope) * landMask,
            coastExposure * smoothstep(terrainRockSlopeStart * 0.60, terrainRockSlopeEnd, surface.slope) * landMask
        );
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

    vec3 grassColor = mix(terrainLowlandColor, vec3(0.16, 0.48, 0.12), 0.34);
    vec3 forestColor = mix(terrainForestColor, vec3(0.035, 0.18, 0.055), 0.34);
    vec3 desertColor = mix(terrainDesertColor, vec3(0.86, 0.66, 0.28), 0.38);
    vec3 rockColor = mix(terrainRockColor, vec3(0.36, 0.37, 0.36), 0.42);
    vec3 beachColor = mix(terrainBeachColor, vec3(0.82, 0.74, 0.48), 0.28);
    vec3 snowColor = mix(terrainSnowColor, vec3(0.96, 0.98, 1.0), 0.30);
    vec3 lowlandTint = grassColor * mix(0.88, 1.10, fbm3(radialUp * 18.0 + 4.1));
    vec3 wetlandColor = mix(grassColor, forestColor, 0.68) * vec3(0.62, 0.88, 0.72);
    vec3 alpineColor = mix(forestColor, mix(rockColor, snowColor, 0.42), 0.58);
    vec3 shallowShelfColor = mix(terrainBeachColor, terrainRockColor, 0.25);
    vec3 sedimentColor = vec3(0.42, 0.36, 0.27);
    vec3 abyssalClayColor = vec3(0.18, 0.16, 0.15);
    vec3 seabedRockColor = terrainRockColor * 0.78;
    vec3 shallowWaterColor = mix(shallowShelfColor, sedimentColor, smoothstep(0.25, 2.5, waterDepth));
    shallowWaterColor = mix(shallowWaterColor, abyssalClayColor, smoothstep(2.0, 10.0, waterDepth));
    shallowWaterColor = mix(shallowWaterColor, seabedRockColor, clamp(wearMask * 0.35 + surface.slope * 0.18, 0.0, 0.55));
    shallowWaterColor = mix(shallowWaterColor, sedimentColor, depositionMask * 0.35);
    vec3 color = (
        beachColor * beachWeight
      + lowlandTint * grassWeight
      + forestColor * forestWeight
      + desertColor * desertWeight
      + rockColor * rockWeight
      + snowColor * snowWeight
      + wetlandColor * wetlandWeight
      + shallowWaterColor * shallowWaterWeight
    ) / max(landWeight + shallowWaterWeight, 0.0001);

    color = mix(color, rockColor * mix(0.86, 1.08, fbm3(radialUp * 42.0 + 9.3)), exposedRockMask * 0.28);
    color = mix(color, rockColor, wearMask * smoothstep(0.26, 0.62, surface.slope) * 0.34);
    color = mix(color, vec3(0.52, 0.43, 0.30), depositionMask * 0.28);
    color = mix(color, vec3(0.06, 0.16, 0.10), channelMask * 0.12);
    color = mix(color, vec3(0.08, 0.20, 0.11), flowMask * 0.22);
    color = mix(color, terrainBeachColor * 0.72, shallowWaterWeight * 0.20);
    color = mix(color, alpineColor, treeLineBlend * 0.48);

    surface.baseColor = clamp(color * colorVariation, vec3(0.0), vec3(2.0));
    return surface;
}
