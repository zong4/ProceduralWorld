#include "PlanetTerrainGenerator.h"

#include <algorithm>
#include <cmath>

#include <FastNoiseLite/FastNoiseLite.h>
#include <glm/gtc/constants.hpp>

namespace
{
FastNoiseLite makeNoise(int seed, float frequency)
{
    FastNoiseLite noise(seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    noise.SetFractalOctaves(5);
    noise.SetFractalLacunarity(2.02f);
    noise.SetFractalGain(0.5f);
    noise.SetFrequency(frequency);
    return noise;
}

FastNoiseLite makeRidgedNoise(int seed, float frequency)
{
    FastNoiseLite noise(seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFractalType(FastNoiseLite::FractalType_Ridged);
    noise.SetFractalOctaves(6);
    noise.SetFractalLacunarity(2.08f);
    noise.SetFractalGain(0.5f);
    noise.SetFrequency(frequency);
    return noise;
}

FastNoiseLite makeDomainWarp(int seed, float frequency)
{
    FastNoiseLite noise(seed);
    noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    noise.SetFractalType(FastNoiseLite::FractalType_DomainWarpIndependent);
    noise.SetFractalOctaves(3);
    noise.SetFractalLacunarity(2.0f);
    noise.SetFractalGain(0.5f);
    noise.SetDomainWarpType(FastNoiseLite::DomainWarpType_OpenSimplex2);
    noise.SetDomainWarpAmp(0.75f);
    noise.SetFrequency(frequency);
    return noise;
}

glm::vec3 warpDirection(const glm::vec3& dir, const FastNoiseLite& warp, float amount)
{
    float x = dir.x;
    float y = dir.y;
    float z = dir.z;
    warp.DomainWarp(x, y, z);
    return glm::normalize(dir + glm::vec3(x - dir.x, y - dir.y, z - dir.z) * amount);
}

float sampleNoise3(const FastNoiseLite& noise, const glm::vec3& p, float scale)
{
    return noise.GetNoise(p.x * scale, p.y * scale, p.z * scale);
}

float ridgeMask3(const FastNoiseLite& noise, const glm::vec3& p, float scale, float sharpness)
{
    const float v = sampleNoise3(noise, p, scale);
    const float ridge = glm::clamp(1.0f - std::abs(v), 0.0f, 1.0f);
    return std::pow(ridge, sharpness);
}

float orogenicBeltMask(const glm::vec3& p)
{
    struct Belt {
        glm::vec3 normal;
        float offset;
        float width;
        float weight;
    };

    const Belt belts[] = {
        { glm::normalize(glm::vec3( 0.28f,  0.82f, -0.50f)),  0.06f, 0.070f, 1.00f },
        { glm::normalize(glm::vec3(-0.74f,  0.25f,  0.62f)), -0.02f, 0.060f, 0.86f },
        { glm::normalize(glm::vec3( 0.64f, -0.34f,  0.69f)),  0.12f, 0.055f, 0.74f },
        { glm::normalize(glm::vec3(-0.18f,  0.47f,  0.86f)), -0.16f, 0.050f, 0.66f },
        { glm::normalize(glm::vec3( 0.91f,  0.19f, -0.36f)),  0.04f, 0.052f, 0.62f }
    };

    float mask = 0.0f;
    for (const Belt& belt : belts) {
        const float distance = std::abs(glm::dot(p, belt.normal) - belt.offset);
        const float core = 1.0f - glm::smoothstep(belt.width, belt.width + 0.055f, distance);
        const float shoulder = 1.0f - glm::smoothstep(belt.width + 0.05f, belt.width + 0.20f, distance);
        mask = std::max(mask, glm::clamp(core * 0.88f + shoulder * 0.38f, 0.0f, 1.0f) * belt.weight);
    }
    return glm::clamp(mask, 0.0f, 1.0f);
}
}

float PlanetTerrainGenerator::terrainHeight(const PlanetRenderSettings& settings, const glm::vec3& sphereDir)
{
    const glm::vec3 dir = glm::normalize(sphereDir);

    const float noiseScale = std::max(settings.terrainNoiseScale, 0.05f);

    static FastNoiseLite continentShape = makeNoise(1337, 0.18f);
    static FastNoiseLite continentDetail = makeNoise(9001, 0.42f);
    static FastNoiseLite continentWarp = makeDomainWarp(4242, 0.34f);
    static FastNoiseLite mountainWarp = makeDomainWarp(9797, 0.16f);
    static FastNoiseLite rangeWarp = makeDomainWarp(3434, 0.11f);
    static FastNoiseLite mountainProvince = makeNoise(2441, 0.24f);
    static FastNoiseLite mountainBackbone = makeRidgedNoise(7777, 0.68f);
    static FastNoiseLite mountainBranches = makeRidgedNoise(2024, 1.28f);
    static FastNoiseLite mountainCrests = makeRidgedNoise(6161, 2.45f);
    static FastNoiseLite mountainRange = makeRidgedNoise(8383, 0.36f);
    static FastNoiseLite mountainBelt = makeRidgedNoise(5959, 0.52f);
    static FastNoiseLite mountainDetail = makeRidgedNoise(2024, 2.15f);
    static FastNoiseLite valleyDetail = makeNoise(6060, 1.95f);
    static FastNoiseLite basinDetail = makeNoise(8080, 0.70f);
    static FastNoiseLite foothillDetail = makeNoise(5050, 0.95f);

    const glm::vec3 warped = warpDirection(dir, continentWarp, 0.18f);
    const glm::vec3 rangeWarped = warpDirection(warped, rangeWarp, 0.20f);
    const glm::vec3 mountainWarped = warpDirection(rangeWarped, mountainWarp, 0.12f);
    const float latitude = 1.0f - std::abs(dir.y);

    const float continentLarge = sampleNoise3(continentShape, warped, noiseScale);
    const float continentMedium = sampleNoise3(continentDetail, warped, noiseScale * 2.3f);
    const float continentMask = glm::smoothstep(-0.10f, 0.46f, continentLarge * 0.66f + continentMedium * 0.34f + latitude * 0.08f);
    const float interiorMask = glm::smoothstep(0.16f, 0.74f, continentMask);

    const float provinceNoise = sampleNoise3(mountainProvince, mountainWarped, noiseScale * 1.40f);
    const float provinceMask = glm::smoothstep(0.24f, 0.80f, continentMask * 0.74f + provinceNoise * 0.28f + latitude * 0.06f);
    const float mountainProvinceMask = provinceMask * interiorMask;

    const float rangeMask = ridgeMask3(mountainRange, rangeWarped, noiseScale * 0.42f, 1.05f);
    const float beltMask = ridgeMask3(mountainBelt, rangeWarped, noiseScale * 0.64f, 1.45f);
    const float tectonicBelt = orogenicBeltMask(rangeWarped);
    const float backboneMask = ridgeMask3(mountainBackbone, mountainWarped, noiseScale * 0.90f, settings.mountainRidgeSharpness * 0.46f);
    const float branchMask = ridgeMask3(mountainBranches, mountainWarped, noiseScale * 1.42f, settings.mountainRidgeSharpness * 0.68f);
    const float crestMask = ridgeMask3(mountainCrests, mountainWarped, noiseScale * 2.70f, settings.mountainRidgeSharpness * 1.12f);
    const float rangeBody = glm::smoothstep(0.14f, 0.70f, rangeMask * 0.44f + beltMask * 0.20f + tectonicBelt * 0.36f);
    const float wideShoulder = glm::smoothstep(0.08f, 0.58f, rangeMask * 0.36f + branchMask * 0.20f + tectonicBelt * 0.34f + provinceMask * 0.10f);
    const float spineField = glm::clamp(backboneMask * 0.56f + branchMask * 0.20f + beltMask * 0.10f + tectonicBelt * 0.14f, 0.0f, 1.0f);
    const float ridgeBody = glm::smoothstep(0.16f, 0.70f, spineField);
    const float ridgeCore = glm::smoothstep(0.34f, 0.86f, backboneMask * 0.72f + beltMask * 0.18f + crestMask * 0.10f);
    const float ridgeShoulder = glm::smoothstep(0.10f, 0.62f, branchMask * 0.70f + rangeBody * 0.30f);
    const float ridgeCrown = glm::smoothstep(0.46f, 0.90f, crestMask) * ridgeCore;

    const float foothillNoise = foothillDetail.GetNoise(
        mountainWarped.x * noiseScale * 3.0f,
        mountainWarped.y * noiseScale * 3.0f,
        mountainWarped.z * noiseScale * 3.0f
    );
    const float valleyNoise = valleyDetail.GetNoise(
        mountainWarped.x * noiseScale * 3.8f,
        mountainWarped.y * noiseScale * 3.8f,
        mountainWarped.z * noiseScale * 3.8f
    );
    const float basinNoiseValue = basinDetail.GetNoise(
        warped.x * noiseScale * 1.05f,
        warped.y * noiseScale * 1.05f,
        warped.z * noiseScale * 1.05f
    );

    float height = continentLarge * 0.38f - 0.02f;
    height += continentMedium * 0.08f * interiorMask;

    const float mountainEnvelope = mountainProvinceMask
        * glm::smoothstep(0.10f, 0.84f, continentMask)
        * glm::smoothstep(0.08f, 0.68f, rangeBody * 0.72f + wideShoulder * 0.28f);
    const float mountainBand = glm::smoothstep(0.04f, 0.68f, mountainEnvelope + wideShoulder * interiorMask * 0.28f);
    const float mountainMass = glm::smoothstep(0.14f, 0.78f, rangeBody * 0.42f + ridgeBody * 0.42f + ridgeShoulder * 0.16f);
    const float mountainStack = mountainMass * 0.48f + ridgeCore * 0.32f + ridgeCrown * 0.20f;
    const float mountainSkirt = glm::smoothstep(0.05f, 0.60f, mountainEnvelope + wideShoulder * 0.22f) * (0.40f + foothillNoise * 0.10f);
    const float mountainIncision = glm::smoothstep(0.18f, 0.86f, 1.0f - mountainEnvelope) * (0.12f + valleyNoise * 0.08f);
    const float basinDrop = glm::smoothstep(0.28f, 0.88f, basinNoiseValue) * (1.0f - interiorMask) * 0.028f;

    const float mountainHeightBoost = 1.75f;
    height += mountainBand * settings.mountainMaskStrength * (0.036f + mountainStack * settings.mountainMaskScale * 0.026f) * mountainHeightBoost;
    height += mountainSkirt * settings.mountainMaskStrength * 0.020f * mountainHeightBoost;
    height += rangeBody * mountainProvinceMask * settings.mountainMaskScale * 0.012f * mountainHeightBoost;
    height += ridgeCore * settings.mountainMaskScale * 0.018f * interiorMask * mountainHeightBoost;
    height += ridgeCrown * settings.mountainMaskScale * 0.010f * mountainEnvelope * mountainHeightBoost;
    height -= mountainIncision * 0.006f;
    height -= basinDrop;

    const float coastDistance = std::abs(height - settings.seaLevelOffset);
    const float coastSoftness = glm::smoothstep(0.0f, 0.070f, coastDistance);
    const float detail = mountainDetail.GetNoise(
        mountainWarped.x * noiseScale * 10.8f,
        mountainWarped.y * noiseScale * 10.8f,
        mountainWarped.z * noiseScale * 10.8f
    );
    const float ridgeDetailMask = glm::smoothstep(0.46f, 0.92f, ridgeCore + ridgeCrown * 0.35f) * coastSoftness * interiorMask;
    const float shoulderDetailMask = glm::smoothstep(0.18f, 0.70f, mountainMass) * (1.0f - ridgeDetailMask * 0.55f) * coastSoftness * interiorMask;
    height += detail * (0.0018f * ridgeDetailMask + 0.00055f * shoulderDetailMask) * mountainHeightBoost;

    return height;
}
