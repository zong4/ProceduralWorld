#include "PlanetProceduralData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>

#include "Instumentor/InstrumentationTimer.hpp"

const std::array<PlanetProceduralData::FaceBasis, 6> PlanetProceduralData::kFaces = {{
    {{ 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f, -1.0f}, { 0.0f,  1.0f,  0.0f}},
    {{-1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f,  1.0f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.0f,  1.0f,  0.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f, -1.0f}},
    {{ 0.0f, -1.0f,  0.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.0f,  0.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.0f,  0.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}}
}};

namespace
{
// 缓存文件版本号和 magic 用来拒绝旧格式/损坏文件。
constexpr char kProceduralCacheMagic[8] = { 'P', 'W', 'C', 'A', 'C', 'H', 'E', '9' };
constexpr std::uint32_t kProceduralCacheVersion = 75;
constexpr int kTerrainChunkMeshResolution = 16;
constexpr int kTerrainChunkMinDepth = 2;
constexpr int kTerrainChunkMaxDepth = 6;
constexpr std::uint32_t kTerrainFeatureCacheVersion = 1;

template <typename T>
bool writeBinary(std::ofstream& file, const T& value)
{
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
    return static_cast<bool>(file);
}

template <typename T>
bool readBinary(std::ifstream& file, T& value)
{
    file.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(file);
}

bool writeFloatArray(std::ofstream& file, const std::vector<float>& values)
{
    const std::uint64_t count = static_cast<std::uint64_t>(values.size());
    if (!writeBinary(file, count)) {
        return false;
    }
    if (count == 0) {
        return true;
    }
    file.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(count * sizeof(float)));
    return static_cast<bool>(file);
}

bool readFloatArray(std::ifstream& file, std::vector<float>& values, std::size_t expectedCount)
{
    std::uint64_t count = 0;
    if (!readBinary(file, count) || count != static_cast<std::uint64_t>(expectedCount)) {
        return false;
    }
    values.resize(expectedCount);
    if (expectedCount == 0) {
        return true;
    }
    file.read(reinterpret_cast<char*>(values.data()), static_cast<std::streamsize>(expectedCount * sizeof(float)));
    return static_cast<bool>(file);
}

bool writeVec4Array(std::ofstream& file, const std::vector<glm::vec4>& values)
{
    const std::uint64_t count = static_cast<std::uint64_t>(values.size());
    if (!writeBinary(file, count)) {
        return false;
    }
    if (count == 0) {
        return true;
    }
    file.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(count * sizeof(glm::vec4)));
    return static_cast<bool>(file);
}

bool readVec4Array(std::ifstream& file, std::vector<glm::vec4>& values, std::size_t expectedCount)
{
    std::uint64_t count = 0;
    if (!readBinary(file, count) || count != static_cast<std::uint64_t>(expectedCount)) {
        return false;
    }
    values.resize(expectedCount);
    if (expectedCount == 0) {
        return true;
    }
    file.read(reinterpret_cast<char*>(values.data()), static_cast<std::streamsize>(expectedCount * sizeof(glm::vec4)));
    return static_cast<bool>(file);
}

struct BiomeWeights {
    float beach = 0.0f;
    float grass = 0.0f;
    float forest = 0.0f;
    float desert = 0.0f;
    float rock = 0.0f;
    float snow = 0.0f;
    float wetland = 0.0f;
    float shallowWater = 0.0f;
};

// 海岸遮蔽度：用多层 value noise 估计某处是否像海湾/内海一样“受保护”。
// 受保护海岸更容易生成沙滩、湿地；暴露海岸更容易生成岩岸。
float coastalShelter(const glm::vec3& sphereDir)
{
    const auto hash = [](const glm::vec3& p) {
        const float h = glm::dot(p, glm::vec3(127.1f, 311.7f, 74.7f));
        return glm::fract(std::sin(h) * 43758.5453123f);
    };
    const auto valueNoise = [&](const glm::vec3& p) {
        const glm::vec3 i = glm::floor(p);
        const glm::vec3 f = glm::fract(p);
        const glm::vec3 u = f * f * (3.0f - 2.0f * f);

        const float n000 = hash(i + glm::vec3(0.0f, 0.0f, 0.0f));
        const float n100 = hash(i + glm::vec3(1.0f, 0.0f, 0.0f));
        const float n010 = hash(i + glm::vec3(0.0f, 1.0f, 0.0f));
        const float n110 = hash(i + glm::vec3(1.0f, 1.0f, 0.0f));
        const float n001 = hash(i + glm::vec3(0.0f, 0.0f, 1.0f));
        const float n101 = hash(i + glm::vec3(1.0f, 0.0f, 1.0f));
        const float n011 = hash(i + glm::vec3(0.0f, 1.0f, 1.0f));
        const float n111 = hash(i + glm::vec3(1.0f, 1.0f, 1.0f));

        const float nx00 = glm::mix(n000, n100, u.x);
        const float nx10 = glm::mix(n010, n110, u.x);
        const float nx01 = glm::mix(n001, n101, u.x);
        const float nx11 = glm::mix(n011, n111, u.x);
        const float nxy0 = glm::mix(nx00, nx10, u.y);
        const float nxy1 = glm::mix(nx01, nx11, u.y);
        return glm::mix(nxy0, nxy1, u.z);
    };
    const auto fbmLocal = [&](glm::vec3 p, int octaves, float lacunarity, float gain) {
        float value = 0.0f;
        float amplitude = 0.5f;
        float total = 0.0f;
        for (int i = 0; i < octaves; ++i) {
            value += valueNoise(p) * amplitude;
            total += amplitude;
            p *= lacunarity;
            amplitude *= gain;
        }
        return value / std::max(total, 0.0001f);
    };

    const glm::vec3 p = sphereDir * 3.7f;
    const float broad = fbmLocal(p + glm::vec3(12.3f, 4.7f, 8.1f), 4, 2.0f, 0.5f);
    const float pocket = fbmLocal(p * 2.35f + glm::vec3(5.7f, 17.9f, 2.8f), 3, 2.1f, 0.5f);
    const float notch = 1.0f - std::abs(valueNoise(p * 5.2f + glm::vec3(31.4f, 7.6f, 19.3f)) * 2.0f - 1.0f);
    const float sheltered = broad * 0.50f + pocket * 0.30f + notch * 0.20f;
    return glm::smoothstep(0.42f, 0.78f, sheltered);
}

// 大尺度气候区域噪声，用于让沙漠、森林、岩地等形成片区，而不是纯纬度条带。
float climateRegionNoise(const glm::vec3& sphereDir, float scale, const glm::vec3& offset)
{
    const auto hash = [](const glm::vec3& p) {
        const float h = glm::dot(p, glm::vec3(157.7f, 219.3f, 91.5f));
        return glm::fract(std::sin(h) * 43758.5453123f);
    };
    const auto valueNoise = [&](const glm::vec3& p) {
        const glm::vec3 i = glm::floor(p);
        const glm::vec3 f = glm::fract(p);
        const glm::vec3 u = f * f * (3.0f - 2.0f * f);

        const float n000 = hash(i + glm::vec3(0.0f, 0.0f, 0.0f));
        const float n100 = hash(i + glm::vec3(1.0f, 0.0f, 0.0f));
        const float n010 = hash(i + glm::vec3(0.0f, 1.0f, 0.0f));
        const float n110 = hash(i + glm::vec3(1.0f, 1.0f, 0.0f));
        const float n001 = hash(i + glm::vec3(0.0f, 0.0f, 1.0f));
        const float n101 = hash(i + glm::vec3(1.0f, 0.0f, 1.0f));
        const float n011 = hash(i + glm::vec3(0.0f, 1.0f, 1.0f));
        const float n111 = hash(i + glm::vec3(1.0f, 1.0f, 1.0f));

        const float nx00 = glm::mix(n000, n100, u.x);
        const float nx10 = glm::mix(n010, n110, u.x);
        const float nx01 = glm::mix(n001, n101, u.x);
        const float nx11 = glm::mix(n011, n111, u.x);
        const float nxy0 = glm::mix(nx00, nx10, u.y);
        const float nxy1 = glm::mix(nx01, nx11, u.y);
        return glm::mix(nxy0, nxy1, u.z);
    };

    glm::vec3 p = sphereDir * scale + offset;
    float value = 0.0f;
    float amplitude = 0.58f;
    float total = 0.0f;
    for (int i = 0; i < 4; ++i) {
        value += valueNoise(p) * amplitude;
        total += amplitude;
        p *= 2.03f;
        amplitude *= 0.48f;
    }
    return value / std::max(total, 0.0001f);
}

// 将地形、水文、气候、坡度和侵蚀信息合成为 8 类 biome 权重。
// 这里输出的是“混合权重”，不是互斥分类；后续 shader 会按权重混色。
BiomeWeights computeBiome(float height,
                          float waterDepth,
                          float shore,
                          float coastalWater,
                          float coastalShelterAmount,
                          const glm::vec3& sphereDir,
                          float seaLevel,
                          float temperature,
                          float moisture,
                          float slope,
                          float channel,
                          float flow,
                          float wear,
                          float deposition)
{
    BiomeWeights biome;

    const float waterMask = glm::smoothstep(0.001f, 0.030f, waterDepth);
    const float landMask = 1.0f - waterMask;
    const float cold = 1.0f - temperature;
    const float dry = 1.0f - moisture;
    const float coastShelter = coastalShelter(sphereDir);
    const float coastExposure = 1.0f - coastShelter;
    const float trueCoast = shore * glm::smoothstep(0.010f, 0.16f, coastalWater);
    const float shelteredCoast = trueCoast * coastalShelterAmount;
    const float beachPatch = glm::smoothstep(
        0.66f,
        0.84f,
        climateRegionNoise(sphereDir, 15.8f, glm::vec3(71.4f, 13.8f, 44.2f)) * 0.64f
      + coastShelter * 0.18f
      + coastalShelterAmount * 0.18f
    );
    const float aridRegion = glm::smoothstep(
        0.50f,
        0.73f,
        climateRegionNoise(sphereDir, 2.20f, glm::vec3(41.2f, 8.7f, 23.4f))
    );
    const float aridPatch = glm::smoothstep(
        0.42f,
        0.68f,
        climateRegionNoise(sphereDir, 5.15f, glm::vec3(81.7f, 19.4f, 5.2f))
    );
    const float forestRegion = glm::smoothstep(
        0.44f,
        0.69f,
        climateRegionNoise(sphereDir, 3.10f, glm::vec3(6.4f, 37.1f, 14.8f))
    );
    const float grassRegion = glm::smoothstep(
        0.30f,
        0.66f,
        climateRegionNoise(sphereDir, 3.75f, glm::vec3(28.3f, 11.6f, 63.9f))
    );
    const float wetlandPatch = glm::smoothstep(
        0.50f,
        0.76f,
        climateRegionNoise(sphereDir, 7.20f, glm::vec3(17.6f, 66.4f, 31.8f))
    );
    const float rockyRegion = glm::smoothstep(
        0.52f,
        0.74f,
        climateRegionNoise(sphereDir, 2.85f, glm::vec3(55.2f, 21.7f, 72.4f))
    );
    const float rockyPatch = glm::smoothstep(
        0.42f,
        0.70f,
        climateRegionNoise(sphereDir, 6.80f, glm::vec3(9.8f, 74.3f, 28.6f))
    );
    const float snowPatch = glm::smoothstep(
        0.38f,
        0.70f,
        climateRegionNoise(sphereDir, 4.60f, glm::vec3(3.7f, 47.2f, 92.1f))
    );
    const float aridCore = aridRegion * glm::mix(0.58f, 1.38f, aridPatch);
    const float hydrology = glm::clamp(channel * 0.35f + flow * 0.45f + deposition * 0.30f + trueCoast * 0.18f, 0.0f, 1.0f);
    const float interiorDry = (1.0f - glm::smoothstep(0.10f, 0.55f, hydrology))
                            * (1.0f - trueCoast);
    const float relativeLandHeight = height - seaLevel;
    const float mountainExclusion = glm::smoothstep(
        0.115f,
        0.245f,
        relativeLandHeight + slope * 0.46f + wear * 0.20f
    );
    const float desertPlain = (1.0f - glm::smoothstep(0.14f, 0.34f, slope))
                            * (1.0f - glm::smoothstep(0.16f, 0.34f, relativeLandHeight))
                            * (1.0f - glm::smoothstep(0.20f, 0.52f, wear))
                            * (1.0f - glm::smoothstep(0.16f, 0.42f, channel))
                            * (1.0f - mountainExclusion);
    const float aridHighland = aridCore
                             * interiorDry
                             * glm::smoothstep(0.28f, 0.62f, slope + relativeLandHeight * 0.30f);
    const float beachReach = glm::mix(0.055f, 0.145f, beachPatch * coastalShelterAmount);
    const float beachHeightBand = glm::smoothstep(0.0f, 0.022f, relativeLandHeight)
                                * (1.0f - glm::smoothstep(beachReach, beachReach + 0.065f, relativeLandHeight));
    const float beachCoastBand = glm::max(
        trueCoast,
        beachHeightBand * glm::smoothstep(0.18f, 0.70f, coastalWater) * coastalShelterAmount
    );

    biome.shallowWater = glm::smoothstep(0.001f, 0.040f, waterDepth)
                       * (1.0f - glm::smoothstep(0.050f, 0.180f, waterDepth));

    biome.beach = landMask
                * beachCoastBand
                * beachPatch
                * (1.0f - glm::smoothstep(0.20f, 0.48f, slope))
                * (1.0f - glm::smoothstep(0.45f, 0.75f, wear))
                * (1.0f - glm::smoothstep(beachReach, beachReach + 0.070f, relativeLandHeight));

    biome.wetland = landMask
                  * (trueCoast * 0.25f + channel * 0.20f + flow * 0.45f + deposition * 0.35f)
                  * glm::mix(0.70f, 1.20f, coastShelter)
                  * glm::mix(0.42f, 1.18f, wetlandPatch)
                  * moisture
                  * (1.0f - glm::smoothstep(0.25f, 0.55f, slope));

    const float highlandMask = glm::smoothstep(0.18f, 0.42f, relativeLandHeight);
    const float exposedCliff = glm::smoothstep(0.24f, 0.48f, slope)
                             * (0.45f + glm::smoothstep(0.08f, 0.30f, relativeLandHeight) * 0.55f);
    const float erodedOutcrop = glm::smoothstep(0.16f, 0.46f, wear)
                              * glm::smoothstep(0.16f, 0.38f, slope + relativeLandHeight * 0.25f);
    const float ridgeOutcrop = highlandMask
                             * glm::smoothstep(0.16f, 0.36f, slope)
                             * glm::smoothstep(0.06f, 0.30f, relativeLandHeight);
    const float exposedRockMask = glm::clamp(
        exposedCliff * 0.82f + erodedOutcrop * 0.58f + ridgeOutcrop * 0.46f,
        0.0f,
        1.0f
    );
    const float dryRockPlain = rockyRegion
                             * glm::mix(0.42f, 1.34f, rockyPatch)
                             * (0.55f + aridCore * 0.65f)
                             * (1.0f - glm::smoothstep(0.30f, 0.60f, moisture))
                             * (1.0f - glm::smoothstep(0.28f, 0.54f, slope))
                             * glm::smoothstep(0.045f, 0.22f, relativeLandHeight)
                             * (1.0f - glm::smoothstep(0.36f, 0.72f, relativeLandHeight))
                             * (1.0f - trueCoast * 0.65f);
    const float rockyCoast = trueCoast
                           * coastExposure
                           * glm::mix(0.62f, 1.45f, rockyPatch)
                           * (0.36f + glm::smoothstep(0.10f, 0.42f, slope + wear * 0.38f) * 0.84f);
    biome.rock = landMask * (
        glm::smoothstep(0.26f, 0.54f, slope) * 0.86f
      + wear * glm::smoothstep(0.16f, 0.42f, slope) * 0.54f
      + channel * glm::smoothstep(0.16f, 0.42f, slope) * 0.14f
      + highlandMask * (0.16f + glm::smoothstep(0.12f, 0.36f, slope) * 0.46f)
      + exposedRockMask * 0.92f
      + dryRockPlain * 1.05f
      + rockyCoast * 0.92f
    );
    biome.rock += landMask
                * trueCoast
                * coastExposure
                * (0.26f + glm::smoothstep(0.22f, 0.62f, slope + rockyPatch * 0.20f) * 0.92f);
    biome.rock += landMask * aridHighland * 0.55f;

    biome.snow = landMask
               * glm::smoothstep(0.32f, 0.58f, relativeLandHeight)
               * glm::smoothstep(0.48f, 0.76f, cold)
               * (0.30f + glm::smoothstep(0.24f, 0.58f, slope) * 0.58f)
               * (1.0f - glm::smoothstep(0.30f, 0.58f, temperature) * 0.75f)
               * glm::mix(0.70f, 1.18f, snowPatch);

    biome.desert = landMask
                 * glm::smoothstep(0.48f, 0.72f, temperature)
                 * glm::smoothstep(0.34f, 0.64f, dry)
                 * interiorDry
                 * glm::mix(0.42f, 1.95f, aridCore)
                 * desertPlain
                 * (1.0f - mountainExclusion);

    const float alpineVegetationCull = glm::smoothstep(
        0.105f,
        0.235f,
        relativeLandHeight + slope * 0.32f
    );
    const float forestAltitudeMask = glm::smoothstep(0.006f, 0.045f, relativeLandHeight)
                                   * (1.0f - glm::smoothstep(0.095f, 0.205f, relativeLandHeight));
    const float forestSlopeMask = 1.0f - glm::smoothstep(0.14f, 0.30f, slope);
    const float mountainForestPenalty = 1.0f - glm::smoothstep(
        0.115f,
        0.255f,
        relativeLandHeight + slope * 0.70f
    );
    const float forestPatch = glm::smoothstep(
        0.38f,
        0.64f,
        climateRegionNoise(sphereDir, 6.35f, glm::vec3(13.8f, 52.1f, 6.6f))
    );
    const float forestViableMask = forestAltitudeMask
                                 * forestSlopeMask
                                 * mountainForestPenalty
                                 * (1.0f - trueCoast * 0.35f)
                                 * (1.0f - biome.snow * 0.70f)
                                 * (1.0f - biome.rock * 0.36f)
                                 * (1.0f - glm::smoothstep(0.18f, 0.46f, wear) * 0.42f);

    biome.forest = landMask
                 * glm::smoothstep(0.34f, 0.58f, moisture)
                 * glm::smoothstep(0.22f, 0.48f, temperature)
                 * forestViableMask
                 * glm::mix(0.48f, 1.72f, forestPatch)
                 * glm::mix(0.52f, 1.92f, forestRegion)
                 * (1.0f - aridCore * dry * 0.72f)
                 * (1.0f - alpineVegetationCull * 0.96f);

    const float highlandForestCull = glm::smoothstep(
        0.115f,
        0.255f,
        relativeLandHeight + slope * 0.55f
    );
    const float culledForest = biome.forest * highlandForestCull;
    biome.forest -= culledForest * 0.85f;
    biome.rock += culledForest * 0.65f;

    biome.grass = landMask
                * glm::smoothstep(0.18f, 0.55f, moisture)
                * glm::smoothstep(0.20f, 0.70f, temperature)
                * (1.0f - glm::smoothstep(0.12f, 0.30f, slope))
                * (1.0f - glm::smoothstep(0.105f, 0.235f, relativeLandHeight + slope * 0.24f))
                * glm::mix(0.68f, 1.26f, grassRegion)
                * (1.0f - biome.desert * 0.88f)
                * (1.0f - biome.forest * 0.52f)
                * (1.0f - biome.rock * 0.55f)
                * (1.0f - biome.snow * 0.85f);
    biome.grass += culledForest
                * (1.0f - glm::smoothstep(0.12f, 0.30f, slope))
                * (1.0f - alpineVegetationCull)
                * 0.12f;

    biome.snow = glm::clamp(biome.snow, 0.0f, 1.0f);
    biome.rock = glm::clamp(biome.rock * (1.0f - biome.snow * 0.28f), 0.0f, 1.0f);
    biome.forest *= (1.0f - biome.desert * 0.85f);
    biome.forest *= (1.0f - biome.snow * 0.95f);
    biome.forest *= (1.0f - biome.rock * 0.36f);
    biome.forest *= (1.0f - exposedRockMask * 0.48f);
    biome.forest *= (1.0f - dryRockPlain * 0.78f);
    biome.forest *= (1.0f - biome.beach * 0.90f);
    biome.grass *= (1.0f - biome.beach * 0.75f);
    biome.grass *= (1.0f - biome.snow * 0.80f);
    biome.grass *= (1.0f - biome.rock * 0.66f);
    biome.grass *= (1.0f - exposedRockMask * 0.34f);
    biome.grass *= (1.0f - dryRockPlain * 0.62f);
    biome.grass *= (1.0f - alpineVegetationCull * 0.94f);
    biome.desert *= (1.0f - exposedRockMask * 0.30f);
    biome.desert *= (1.0f - dryRockPlain * 0.48f);
    biome.desert *= (1.0f - biome.wetland * 0.80f);
    biome.beach *= (1.0f - biome.rock * 0.58f);
    biome.beach *= (1.0f - rockyCoast * 0.82f);
    biome.beach *= glm::mix(0.42f, 1.18f, coastalShelterAmount);

    const float landSum = biome.beach
                        + biome.grass
                        + biome.forest
                        + biome.desert
                        + biome.rock
                        + biome.snow
                        + biome.wetland;

    if (landSum > 0.00001f) {
        const float inv = landMask / landSum;
        biome.beach *= inv;
        biome.grass *= inv;
        biome.forest *= inv;
        biome.desert *= inv;
        biome.rock *= inv;
        biome.snow *= inv;
        biome.wetland *= inv;
    } else {
        biome.grass = landMask;
    }

    biome.shallowWater = glm::clamp(biome.shallowWater, 0.0f, 1.0f);
    return biome;
}
}

bool PlanetProceduralData::saveCache(const char* path) const
{
    PROFILE_SCOPE("Save Procedural Cache");
    if (!generated_ || resolution_ <= 0) {
        return false;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    file.write(kProceduralCacheMagic, sizeof(kProceduralCacheMagic));
    // 文件头保存统计信息，随后逐 face 写入所有标量/向量层。
    const std::int32_t resolution = resolution_;
    if (!writeBinary(file, kProceduralCacheVersion)
        || !writeBinary(file, resolution)
        || !writeBinary(file, minHeight_)
        || !writeBinary(file, maxHeight_)
        || !writeBinary(file, maxWaterDepth_)
        || !writeBinary(file, waterCoverage_)
        || !writeBinary(file, shoreCoverage_)) {
        return false;
    }

    for (const FaceData& faceData : faces_) {
        const std::int32_t faceResolution = faceData.resolution;
        if (!writeBinary(file, faceResolution)
            || !writeFloatArray(file, faceData.height)
            || !writeFloatArray(file, faceData.waterDepth)
            || !writeFloatArray(file, faceData.shoreMask)
            || !writeFloatArray(file, faceData.erosionMask)
            || !writeFloatArray(file, faceData.channelMask)
            || !writeFloatArray(file, faceData.flowMask)
            || !writeFloatArray(file, faceData.wearMask)
            || !writeFloatArray(file, faceData.depositionMask)
            || !writeFloatArray(file, faceData.temperature)
            || !writeFloatArray(file, faceData.moisture)
            || !writeFloatArray(file, faceData.regionId)
            || !writeFloatArray(file, faceData.featureMask)
            || !writeFloatArray(file, faceData.meshDensity)
            || !writeFloatArray(file, faceData.geometricError)
            || !writeVec4Array(file, faceData.biomeWeightA)
            || !writeVec4Array(file, faceData.biomeWeightB)
            || !writeVec4Array(file, faceData.domainWeight)) {
            return false;
        }
    }

    return static_cast<bool>(file);
}

bool PlanetProceduralData::loadCache(const char* path, const PlanetRenderSettings& settings)
{
    PROFILE_SCOPE("Load Procedural Cache");
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    char magic[8] = {};
    file.read(magic, sizeof(magic));
    if (!file || !std::equal(std::begin(magic), std::end(magic), std::begin(kProceduralCacheMagic))) {
        return false;
    }

    std::uint32_t version = 0;
    std::int32_t resolution = 0;
    // 缓存不做跨版本兼容；算法或字段变更时 bump 版本并强制重新生成。
    if (!readBinary(file, version)
        || version != kProceduralCacheVersion
        || !readBinary(file, resolution)
        || resolution < 16
        || resolution > 512) {
        return false;
    }

    float loadedMinHeight = 0.0f;
    float loadedMaxHeight = 0.0f;
    float loadedMaxWaterDepth = 0.0f;
    float loadedWaterCoverage = 0.0f;
    float loadedShoreCoverage = 0.0f;
    if (!readBinary(file, loadedMinHeight)
        || !readBinary(file, loadedMaxHeight)
        || !readBinary(file, loadedMaxWaterDepth)
        || !readBinary(file, loadedWaterCoverage)
        || !readBinary(file, loadedShoreCoverage)) {
        return false;
    }

    const std::size_t expectedCount = static_cast<std::size_t>(resolution * resolution);
    std::array<FaceData, 6> loadedFaces{};
    for (FaceData& faceData : loadedFaces) {
        std::int32_t faceResolution = 0;
        if (!readBinary(file, faceResolution) || faceResolution != resolution) {
            return false;
        }

        faceData.resolution = resolution;
        if (!readFloatArray(file, faceData.height, expectedCount)
            || !readFloatArray(file, faceData.waterDepth, expectedCount)
            || !readFloatArray(file, faceData.shoreMask, expectedCount)
            || !readFloatArray(file, faceData.erosionMask, expectedCount)
            || !readFloatArray(file, faceData.channelMask, expectedCount)
            || !readFloatArray(file, faceData.flowMask, expectedCount)
            || !readFloatArray(file, faceData.wearMask, expectedCount)
            || !readFloatArray(file, faceData.depositionMask, expectedCount)
            || !readFloatArray(file, faceData.temperature, expectedCount)
            || !readFloatArray(file, faceData.moisture, expectedCount)
            || !readFloatArray(file, faceData.regionId, expectedCount)
            || !readFloatArray(file, faceData.featureMask, expectedCount)
            || !readFloatArray(file, faceData.meshDensity, expectedCount)
            || !readFloatArray(file, faceData.geometricError, expectedCount)
            || !readVec4Array(file, faceData.biomeWeightA, expectedCount)
            || !readVec4Array(file, faceData.biomeWeightB, expectedCount)
            || !readVec4Array(file, faceData.domainWeight, expectedCount)) {
            return false;
        }
    }

    settings_ = settings;
    resolution_ = resolution;
    faces_ = std::move(loadedFaces);
    minHeight_ = loadedMinHeight;
    maxHeight_ = loadedMaxHeight;
    maxWaterDepth_ = loadedMaxWaterDepth;
    waterCoverage_ = loadedWaterCoverage;
    shoreCoverage_ = loadedShoreCoverage;
    buildTerrainSkeletons(settings_);
    buildTerrainFeatureSegments(settings_);
    buildTerrainChunks(settings_);
    generated_ = true;
    return true;
}

PlanetGlobalHeightField PlanetProceduralData::globalHeightField() const
{
    // 将内部 FaceData 转成通用高度场结构，供未来局部 LOD/工具链复用。
    PlanetGlobalHeightField heightField;
    heightField.faceResolution = resolution_;
    heightField.minHeight = minHeight_;
    heightField.maxHeight = maxHeight_;
    heightField.maxWaterDepth = maxWaterDepth_;
    heightField.waterCoverage = waterCoverage_;
    heightField.shoreCoverage = shoreCoverage_;

    const auto scalarIndex = [](PlanetScalarLayer layer) {
        return static_cast<std::size_t>(layer);
    };
    const auto vectorIndex = [](PlanetVectorLayer layer) {
        return static_cast<std::size_t>(layer);
    };

    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        const FaceData& source = faces_[faceIndex];
        PlanetHeightFieldFace& target = heightField.faces[faceIndex];
        target.resolution = source.resolution;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::Height)] = source.height;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::WaterDepth)] = source.waterDepth;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::ShoreMask)] = source.shoreMask;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::ErosionMask)] = source.erosionMask;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::ChannelMask)] = source.channelMask;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::FlowMask)] = source.flowMask;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::WearMask)] = source.wearMask;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::DepositionMask)] = source.depositionMask;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::Temperature)] = source.temperature;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::Moisture)] = source.moisture;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::RegionId)] = source.regionId;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::FeatureMask)] = source.featureMask;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::MeshDensity)] = source.meshDensity;
        target.scalarLayers[scalarIndex(PlanetScalarLayer::GeometricError)] = source.geometricError;
        target.vectorLayers[vectorIndex(PlanetVectorLayer::BiomeWeightA)] = source.biomeWeightA;
        target.vectorLayers[vectorIndex(PlanetVectorLayer::BiomeWeightB)] = source.biomeWeightB;
        target.vectorLayers[vectorIndex(PlanetVectorLayer::DomainWeight)] = source.domainWeight;
    }

    return heightField;
}

void PlanetProceduralData::clear()
{
    generated_ = false;
    resolution_ = 0;
    faces_ = {};
    terrainChunks_.clear();
    terrainFeatureSegments_.clear();
    terrainSkeletons_.clear();
    terrainPeakNodes_.clear();
    minHeight_ = 0.0f;
    maxHeight_ = 0.0f;
    maxWaterDepth_ = 0.0f;
    waterCoverage_ = 0.0f;
    shoreCoverage_ = 0.0f;
}

void PlanetProceduralData::generate(const PlanetRenderSettings& settings, int faceResolution)
{
    generate(settings, faceResolution, ProgressCallback{});
}

void PlanetProceduralData::generate(const PlanetRenderSettings& settings,
                                    int faceResolution,
                                    const ProgressCallback& progressCallback)
{
    PROFILE_SCOPE("Generate Procedural Planet");
    settings_ = settings;
    resolution_ = std::clamp(faceResolution, 16, 512);
    generated_ = false;

    minHeight_ = std::numeric_limits<float>::max();
    maxHeight_ = std::numeric_limits<float>::lowest();
    maxWaterDepth_ = 0.0f;

    const int erosionIterations = std::clamp(settings.erosionIterations, 0, 256);
    const float erosionStrength = std::max(settings.erosionStrength, 0.0f);
    const float thermalStrength = std::max(settings.erosionThermalStrength, 0.0f);
    const bool erosionActive = erosionIterations > 0 && (erosionStrength > 0.0f || thermalStrength > 0.0f);
    const int thermalIterations = erosionActive && thermalStrength > 0.0f
        ? std::clamp(erosionIterations / 3, 1, 80)
        : 0;
    std::array<int, static_cast<std::size_t>(GenerationModule::Count)> moduleTotals{};
    std::array<int, static_cast<std::size_t>(GenerationModule::Count)> moduleCompleted{};
    moduleTotals[static_cast<std::size_t>(GenerationModule::BaseTerrain)] = resolution_ * 6 + 8;
    moduleTotals[static_cast<std::size_t>(GenerationModule::InitialClimate)] = resolution_ * 6 + 1;
    moduleTotals[static_cast<std::size_t>(GenerationModule::InitialBiomes)] = resolution_ * 6 + 2;
    moduleTotals[static_cast<std::size_t>(GenerationModule::BiomeTerrain)] = resolution_ * 6 + 1;
    moduleTotals[static_cast<std::size_t>(GenerationModule::Erosion)] =
        erosionActive ? erosionIterations + thermalIterations + 8 : 0;
    moduleTotals[static_cast<std::size_t>(GenerationModule::FinalClimate)] = resolution_ * 6 + 3;
    moduleTotals[static_cast<std::size_t>(GenerationModule::FinalBiomes)] = resolution_ * 6 + 2;
    moduleTotals[static_cast<std::size_t>(GenerationModule::MeshPlanning)] = resolution_ * 6 + 1;
    moduleTotals[static_cast<std::size_t>(GenerationModule::Finalize)] = 1 + 6;

    int totalSteps = 0;
    for (int moduleTotal : moduleTotals) {
        totalSteps += moduleTotal;
    }
    int completedSteps = 0;
    GenerationModule activeModule = GenerationModule::BaseTerrain;
    const auto reportProgress = [&](const char* status) {
        if (progressCallback) {
            const std::size_t moduleIndex = static_cast<std::size_t>(activeModule);
            progressCallback(GenerationProgress{
                std::min(completedSteps, totalSteps),
                std::max(totalSteps, 1),
                activeModule,
                moduleCompleted[moduleIndex],
                std::max(moduleTotals[moduleIndex], 1),
                status
            });
        }
    };
    const auto advanceModuleProgress = [&](GenerationModule module, const char* status) {
        activeModule = module;
        completedSteps = std::min(completedSteps + 1, totalSteps);
        const std::size_t moduleIndex = static_cast<std::size_t>(module);
        moduleCompleted[moduleIndex] = std::min(moduleCompleted[moduleIndex] + 1, moduleTotals[moduleIndex]);
        reportProgress(status);
    };
    const auto advanceErosionProgress = [&](const char* status) {
        advanceModuleProgress(GenerationModule::Erosion, status);
    };

    reportProgress("Preparing terrain buffers");

    // 第一阶段：为 6 个 cube face 采样基础高度。
    // 每个 texel 先映射到球面方向，再调用 terrainHeight 得到归一化高度。
    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FaceData& faceData = faces_[faceIndex];
        faceData.resolution = resolution_;
        faceData.height.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.waterDepth.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.shoreMask.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.erosionMask.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.channelMask.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.flowMask.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.wearMask.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.depositionMask.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.temperature.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.moisture.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.regionId.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.featureMask.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.meshDensity.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.geometricError.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.biomeWeightA.assign(static_cast<std::size_t>(resolution_ * resolution_), glm::vec4(0.0f));
        faceData.biomeWeightB.assign(static_cast<std::size_t>(resolution_ * resolution_), glm::vec4(0.0f));
        faceData.domainWeight.assign(static_cast<std::size_t>(resolution_ * resolution_), glm::vec4(0.0f));

        for (int y = 0; y < resolution_; ++y) {
            for (int x = 0; x < resolution_; ++x) {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution_),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution_)
                );
                const glm::vec3 sphereDir = cubeSphereDirection(kFaces[faceIndex], uv);
                const float normalizedHeight = terrainHeight(settings, sphereDir);
                const std::size_t index = static_cast<std::size_t>(y * resolution_ + x);

                faceData.height[index] = normalizedHeight;
            }
            advanceModuleProgress(GenerationModule::BaseTerrain, "Generating continents, highlands, and mountain belts");
        }
    }

    // 后续阶段故意多次“水文/biome -> 塑形/侵蚀 -> 水文/biome”循环，
    // 让地形、河道、湿度和材质分布互相影响，而不是单向涂色。
    buildTerrainSkeletons(settings);
    buildTerrainPeakNodes(settings);
    applyHeightfieldNoiseLayers(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::BaseTerrain, status);
    });
    removeSmallTerrainSpikes(settings, 1, 0.018f, 0.28f);
    relaxExtremeTerrainSlopes(settings, 1, 0.042f, 0.16f);
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::BaseTerrain, "Blending structural mountain fields");

    removeSmallTerrainSpikes(settings, 1, 0.016f, 0.24f);
    smoothSmallTerrainBumps(settings, 1, 0.08f);
    relaxExtremeTerrainSlopes(settings, 1, 0.038f, 0.14f);
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::BaseTerrain, "Blending broad mountain seams");

    computeWaterClimateFields(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::InitialClimate, status);
    });
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::InitialClimate, "Blending initial climate seams");

    computeBiomeWeights(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::InitialBiomes, status);
    });
    smoothBiomeWeights(1, 0.42f);
    advanceModuleProgress(GenerationModule::InitialBiomes, "Smoothing initial biome transitions");
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::InitialBiomes, "Blending initial biome seams");

    refineTerrainFromBiomeWeights(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::BiomeTerrain, status);
    });
    removeSmallTerrainSpikes(settings, 1, 0.010f, 0.36f);
    smoothSmallTerrainBumps(settings, 2, 0.22f);
    relaxExtremeTerrainSlopes(settings, 1, 0.024f, 0.35f);
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::BiomeTerrain, "Blending biome-shaped terrain seams");

    applyErosion(settings, advanceErosionProgress);
    extractPrimaryRiver(settings, advanceErosionProgress);
    removeSmallTerrainSpikes(settings, 1, 0.011f, 0.32f);
    smoothSmallTerrainBumps(settings, 1, 0.18f);
    relaxExtremeTerrainSlopes(settings, 2, 0.026f, 0.32f);
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::Erosion, "Blending primary river seams");

    computeWaterClimateFields(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::FinalClimate, status);
    });

    fixCubeFaceSeams();
    updateHydrologyMoisture(settings);
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::FinalClimate, "Updating moisture from rivers and coasts");
    advanceModuleProgress(GenerationModule::FinalClimate, "Blending final climate seams");
    computeBiomeWeights(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::FinalBiomes, status);
    });
    smoothBiomeWeights(1, 0.42f);
    advanceModuleProgress(GenerationModule::FinalBiomes, "Smoothing final biome transitions");
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::FinalBiomes, "Blending final biome seams");

    computeMeshPlanningFields(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::MeshPlanning, status);
    });
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::MeshPlanning, "Blending mesh planning seams");

    minHeight_ = std::numeric_limits<float>::max();
    maxHeight_ = std::numeric_limits<float>::lowest();
    maxWaterDepth_ = 0.0f;
    std::size_t waterSamples = 0;
    std::size_t shoreSamples = 0;
    std::size_t totalSamples = 0;
    for (const FaceData& faceData : faces_) {
        for (std::size_t i = 0; i < faceData.height.size(); ++i) {
            minHeight_ = std::min(minHeight_, faceData.height[i]);
            maxHeight_ = std::max(maxHeight_, faceData.height[i]);
            maxWaterDepth_ = std::max(maxWaterDepth_, faceData.waterDepth[i]);
            waterSamples += faceData.waterDepth[i] > 0.0f ? 1 : 0;
            shoreSamples += faceData.shoreMask[i] > 0.05f ? 1 : 0;
            ++totalSamples;
        }
        advanceModuleProgress(GenerationModule::Finalize, "Collecting height, ocean, and coast coverage");
    }

    if (totalSamples > 0) {
        waterCoverage_ = static_cast<float>(waterSamples) / static_cast<float>(totalSamples);
        shoreCoverage_ = static_cast<float>(shoreSamples) / static_cast<float>(totalSamples);
    } else {
        waterCoverage_ = 0.0f;
        shoreCoverage_ = 0.0f;
    }

    buildTerrainFeatureSegments(settings);
    buildTerrainChunks(settings);
    generated_ = true;
    reportProgress("Generation complete");
}

void PlanetProceduralData::fixCubeFaceSeams()
{
    if (resolution_ <= 1) {
        return;
    }

    const int n = resolution_;
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };
    const int geometrySeamRings = 1;
    const int materialSeamRings = 1;

    // 标量层接缝修复：把 face 边界和跨 face 邻居取平均。
    // neighborCell 支持越界 UV 映射到相邻 face，因此这里不需要手写 12 条边关系。
    const auto reconcileField = [&](std::vector<float> FaceData::* field, int seamRings) {
        std::array<std::vector<float>, 6> updated;
        for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
            updated[faceIndex] = faces_[faceIndex].*field;
        }

        const auto reconcilePair = [&](int faceIndex, std::size_t currentIndex, const CellRef& neighbor) {
            const std::size_t currentFace = static_cast<std::size_t>(faceIndex);
            const std::size_t neighborFace = static_cast<std::size_t>(neighbor.face);
            if (currentFace == neighborFace && currentIndex == neighbor.index) {
                return;
            }
            const float currentValue = (faces_[currentFace].*field)[currentIndex];
            const float neighborValue = (faces_[neighborFace].*field)[neighbor.index];
            const float average = 0.5f * (currentValue + neighborValue);
            updated[currentFace][currentIndex] = average;
            updated[neighborFace][neighbor.index] = average;
        };

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            for (int ring = 0; ring < seamRings; ++ring) {
                for (int i = 0; i < n; ++i) {
                    reconcilePair(faceIndex, indexOf(ring, i), neighborCell(faceIndex, -1 - ring, i, n));
                    reconcilePair(faceIndex, indexOf(n - 1 - ring, i), neighborCell(faceIndex, n + ring, i, n));
                    reconcilePair(faceIndex, indexOf(i, ring), neighborCell(faceIndex, i, -1 - ring, n));
                    reconcilePair(faceIndex, indexOf(i, n - 1 - ring), neighborCell(faceIndex, i, n + ring, n));
                }
            }
        }

        for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
            faces_[faceIndex].*field = std::move(updated[faceIndex]);
        }
    };
    // vec4 层接缝修复，逻辑与标量层一致，用于 biome 权重。
    const auto reconcileVec4Field = [&](std::vector<glm::vec4> FaceData::* field, int seamRings) {
        std::array<std::vector<glm::vec4>, 6> updated;
        for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
            updated[faceIndex] = faces_[faceIndex].*field;
        }

        const auto reconcilePair = [&](int faceIndex, std::size_t currentIndex, const CellRef& neighbor) {
            const std::size_t currentFace = static_cast<std::size_t>(faceIndex);
            const std::size_t neighborFace = static_cast<std::size_t>(neighbor.face);
            if (currentFace == neighborFace && currentIndex == neighbor.index) {
                return;
            }
            const glm::vec4 currentValue = (faces_[currentFace].*field)[currentIndex];
            const glm::vec4 neighborValue = (faces_[neighborFace].*field)[neighbor.index];
            const glm::vec4 average = 0.5f * (currentValue + neighborValue);
            updated[currentFace][currentIndex] = average;
            updated[neighborFace][neighbor.index] = average;
        };

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            for (int ring = 0; ring < seamRings; ++ring) {
                for (int i = 0; i < n; ++i) {
                    reconcilePair(faceIndex, indexOf(ring, i), neighborCell(faceIndex, -1 - ring, i, n));
                    reconcilePair(faceIndex, indexOf(n - 1 - ring, i), neighborCell(faceIndex, n + ring, i, n));
                    reconcilePair(faceIndex, indexOf(i, ring), neighborCell(faceIndex, i, -1 - ring, n));
                    reconcilePair(faceIndex, indexOf(i, n - 1 - ring), neighborCell(faceIndex, i, n + ring, n));
                }
            }
        }

        for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
            faces_[faceIndex].*field = std::move(updated[faceIndex]);
        }
    };

    reconcileField(&FaceData::height, geometrySeamRings);
    reconcileField(&FaceData::waterDepth, materialSeamRings);
    reconcileField(&FaceData::shoreMask, materialSeamRings);
    reconcileField(&FaceData::erosionMask, materialSeamRings);
    reconcileField(&FaceData::channelMask, materialSeamRings);
    reconcileField(&FaceData::flowMask, materialSeamRings);
    reconcileField(&FaceData::wearMask, materialSeamRings);
    reconcileField(&FaceData::depositionMask, materialSeamRings);
    reconcileField(&FaceData::temperature, materialSeamRings);
    reconcileField(&FaceData::moisture, materialSeamRings);
    reconcileField(&FaceData::regionId, materialSeamRings);
    reconcileField(&FaceData::featureMask, materialSeamRings);
    reconcileField(&FaceData::meshDensity, materialSeamRings);
    reconcileField(&FaceData::geometricError, materialSeamRings);
    reconcileVec4Field(&FaceData::biomeWeightA, materialSeamRings);
    reconcileVec4Field(&FaceData::biomeWeightB, materialSeamRings);
    reconcileVec4Field(&FaceData::domainWeight, materialSeamRings);
}

float sampleFaceLayerBilinear(const std::vector<float>& layer, int resolution, const glm::vec2& uv)
{
    if (layer.empty() || resolution <= 1) {
        return 0.0f;
    }

    const float x = glm::clamp(uv.x, 0.0f, 1.0f) * static_cast<float>(resolution - 1);
    const float y = glm::clamp(uv.y, 0.0f, 1.0f) * static_cast<float>(resolution - 1);
    const int x0 = glm::clamp(static_cast<int>(std::floor(x)), 0, resolution - 1);
    const int y0 = glm::clamp(static_cast<int>(std::floor(y)), 0, resolution - 1);
    const int x1 = glm::min(x0 + 1, resolution - 1);
    const int y1 = glm::min(y0 + 1, resolution - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const auto at = [&](int sx, int sy) {
        return layer[static_cast<std::size_t>(sy * resolution + sx)];
    };
    const float bottom = glm::mix(at(x0, y0), at(x1, y0), tx);
    const float top = glm::mix(at(x0, y1), at(x1, y1), tx);
    return glm::mix(bottom, top, ty);
}

float distanceToSegmentUv(const glm::vec2& point, const glm::vec2& a, const glm::vec2& b, float& along)
{
    const glm::vec2 ab = b - a;
    const float lengthSq = glm::dot(ab, ab);
    if (lengthSq <= 1.0e-8f) {
        along = 0.0f;
        return glm::length(point - a);
    }
    along = glm::clamp(glm::dot(point - a, ab) / lengthSq, 0.0f, 1.0f);
    return glm::length(point - (a + ab * along));
}

float distanceToSegmentOnSphere(const glm::vec3& pointDir,
                                const glm::vec3& aDir,
                                const glm::vec3& bDir,
                                float& along)
{
    const glm::vec3 p = glm::normalize(pointDir);
    const glm::vec3 a = glm::normalize(aDir);
    const glm::vec3 b = glm::normalize(bDir);
    const float abDot = glm::clamp(glm::dot(a, b), -1.0f, 1.0f);
    const float totalAngle = std::acos(abDot);
    if (totalAngle <= 1.0e-5f) {
        along = 0.0f;
        return std::acos(glm::clamp(glm::dot(p, a), -1.0f, 1.0f));
    }

    const glm::vec3 greatCircleNormal = glm::normalize(glm::cross(a, b));
    const float signedAlongAngle = std::atan2(glm::dot(glm::cross(a, p), greatCircleNormal), glm::dot(a, p));
    along = glm::clamp(signedAlongAngle / totalAngle, 0.0f, 1.0f);

    if (signedAlongAngle < 0.0f || signedAlongAngle > totalAngle) {
        const float distanceA = std::acos(glm::clamp(glm::dot(p, a), -1.0f, 1.0f));
        const float distanceB = std::acos(glm::clamp(glm::dot(p, b), -1.0f, 1.0f));
        along = distanceA <= distanceB ? 0.0f : 1.0f;
        return std::min(distanceA, distanceB);
    }

    return std::abs(std::asin(glm::clamp(glm::dot(p, greatCircleNormal), -1.0f, 1.0f)));
}

glm::vec4 sampleFaceVec4LayerBilinear(const std::vector<glm::vec4>& layer, int resolution, const glm::vec2& uv)
{
    if (layer.empty() || resolution <= 1) {
        return glm::vec4(0.0f);
    }

    const float x = glm::clamp(uv.x, 0.0f, 1.0f) * static_cast<float>(resolution - 1);
    const float y = glm::clamp(uv.y, 0.0f, 1.0f) * static_cast<float>(resolution - 1);
    const int x0 = glm::clamp(static_cast<int>(std::floor(x)), 0, resolution - 1);
    const int y0 = glm::clamp(static_cast<int>(std::floor(y)), 0, resolution - 1);
    const int x1 = glm::min(x0 + 1, resolution - 1);
    const int y1 = glm::min(y0 + 1, resolution - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const auto at = [&](int sx, int sy) {
        return layer[static_cast<std::size_t>(sy * resolution + sx)];
    };
    const glm::vec4 bottom = glm::mix(at(x0, y0), at(x1, y0), tx);
    const glm::vec4 top = glm::mix(at(x0, y1), at(x1, y1), tx);
    return glm::mix(bottom, top, ty);
}

std::size_t PlanetProceduralData::terrainFeatureSegmentCount(TerrainFeatureType type) const
{
    return static_cast<std::size_t>(std::count_if(
        terrainFeatureSegments_.begin(),
        terrainFeatureSegments_.end(),
        [type](const TerrainFeatureSegment& segment) {
            return segment.type == type;
        }
    ));
}

void PlanetProceduralData::buildTerrainFeatureSegments(const PlanetRenderSettings& settings)
{
    PROFILE_SCOPE("Build Terrain Feature Segments");
    terrainFeatureSegments_.clear();
    if (resolution_ <= 2) {
        return;
    }

    const auto appendSegment = [&](TerrainFeatureType type,
                                   int faceIndex,
                                   const glm::vec2& uvA,
                                   const glm::vec2& uvB,
                                   float strength) {
        TerrainFeatureSegment segment;
        segment.type = type;
        segment.faceIndex = faceIndex;
        segment.uvA = glm::clamp(uvA, glm::vec2(0.0f), glm::vec2(1.0f));
        segment.uvB = glm::clamp(uvB, glm::vec2(0.0f), glm::vec2(1.0f));
        segment.sphereDirA = cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], segment.uvA);
        segment.sphereDirB = cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], segment.uvB);
        segment.strength = glm::clamp(strength, 0.0f, 1.0f);
        terrainFeatureSegments_.push_back(segment);
    };

    const auto addSegment = [&](TerrainFeatureType type,
                                int faceIndex,
                                const glm::vec2& uvA,
                                const glm::vec2& uvB,
                                float strength) {
        appendSegment(type, faceIndex, uvA, uvB, strength);
    };

    const auto uvAtCell = [&](int x, int y) {
        return glm::vec2(
            (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution_),
            (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution_)
        );
    };

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const FaceData& face = faces_[static_cast<std::size_t>(faceIndex)];
        const auto indexOf = [&](int x, int y) {
            return static_cast<std::size_t>(y * resolution_ + x);
        };

        for (int y = 1; y < resolution_ - 1; y += 2) {
            for (int x = 1; x < resolution_ - 1; x += 2) {
                const std::size_t index = indexOf(x, y);
                const float channel = face.channelMask[index];
                const float flow = face.flowMask[index];
                if (channel > 0.34f || flow > 0.58f) {
                    int bestX = x;
                    int bestY = y;
                    float bestHeight = face.height[index];
                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox == 0 && oy == 0) {
                                continue;
                            }
                            const float h = face.height[indexOf(x + ox, y + oy)];
                            if (h < bestHeight) {
                                bestHeight = h;
                                bestX = x + ox;
                                bestY = y + oy;
                            }
                        }
                    }
                    if (bestX != x || bestY != y) {
                        addSegment(TerrainFeatureType::River,
                                   faceIndex,
                                   uvAtCell(x, y),
                                   uvAtCell(bestX, bestY),
                                   glm::clamp(channel * 0.72f + flow * 0.46f, 0.0f, 1.0f));
                    }
                }
            }
        }

        const float seaLevel = settings.seaLevelOffset;
        for (int y = 0; y < resolution_ - 1; y += 2) {
            for (int x = 0; x < resolution_ - 1; x += 2) {
                const float h00 = face.height[indexOf(x, y)] - seaLevel;
                const float h10 = face.height[indexOf(x + 1, y)] - seaLevel;
                const float h01 = face.height[indexOf(x, y + 1)] - seaLevel;
                const float h11 = face.height[indexOf(x + 1, y + 1)] - seaLevel;
                const bool mixed = (h00 < 0.0f || h10 < 0.0f || h01 < 0.0f || h11 < 0.0f)
                                && (h00 > 0.0f || h10 > 0.0f || h01 > 0.0f || h11 > 0.0f);
                if (mixed) {
                    const glm::vec2 center = (uvAtCell(x, y) + uvAtCell(x + 1, y + 1)) * 0.5f;
                    const bool horizontal = glm::abs((h00 + h10) - (h01 + h11)) > glm::abs((h00 + h01) - (h10 + h11));
                    const glm::vec2 half = horizontal
                        ? glm::vec2(0.42f / static_cast<float>(resolution_), 0.0f)
                        : glm::vec2(0.0f, 0.42f / static_cast<float>(resolution_));
                    const float shoreStrength = glm::max(glm::max(face.shoreMask[indexOf(x, y)], face.shoreMask[indexOf(x + 1, y)]),
                                                         glm::max(face.shoreMask[indexOf(x, y + 1)], face.shoreMask[indexOf(x + 1, y + 1)]));
                    addSegment(TerrainFeatureType::Coast, faceIndex, center - half, center + half, glm::max(shoreStrength, 0.55f));
                }
            }
        }

        for (int y = 2; y < resolution_ - 2; y += 2) {
            for (int x = 2; x < resolution_ - 2; x += 2) {
                const std::size_t index = indexOf(x, y);
                const float height = face.height[index];
                const float ridgeDomain = face.domainWeight.empty() ? 0.0f : face.domainWeight[index].x;
                const float left = face.height[indexOf(x - 1, y)];
                const float right = face.height[indexOf(x + 1, y)];
                const float down = face.height[indexOf(x, y - 1)];
                const float up = face.height[indexOf(x, y + 1)];
                const bool ridgeX = height > left && height > right && glm::abs(up - down) > glm::abs(left - right);
                const bool ridgeY = height > down && height > up && glm::abs(left - right) >= glm::abs(up - down);
                if ((ridgeX || ridgeY) && ridgeDomain > 0.38f && height > seaLevel + 0.04f) {
                    const glm::vec2 center = uvAtCell(x, y);
                    const glm::vec2 half = ridgeX
                        ? glm::vec2(0.0f, 0.75f / static_cast<float>(resolution_))
                        : glm::vec2(0.75f / static_cast<float>(resolution_), 0.0f);
                    addSegment(TerrainFeatureType::Ridge, faceIndex, center - half, center + half, ridgeDomain);
                }
            }
        }

        for (int y = 1; y < resolution_ - 1; y += 2) {
            for (int x = 1; x < resolution_ - 1; x += 2) {
                const std::size_t index = indexOf(x, y);
                const float wear = face.wearMask[index];
                const float deposition = face.depositionMask[index];
                const float gradX = glm::abs(face.wearMask[indexOf(x + 1, y)] - face.wearMask[indexOf(x - 1, y)])
                                  + glm::abs(face.depositionMask[indexOf(x + 1, y)] - face.depositionMask[indexOf(x - 1, y)]);
                const float gradY = glm::abs(face.wearMask[indexOf(x, y + 1)] - face.wearMask[indexOf(x, y - 1)])
                                  + glm::abs(face.depositionMask[indexOf(x, y + 1)] - face.depositionMask[indexOf(x, y - 1)]);
                const float erosionStrength = glm::max(wear, deposition);
                if (erosionStrength > 0.28f && glm::max(gradX, gradY) > 0.16f) {
                    const glm::vec2 center = uvAtCell(x, y);
                    const glm::vec2 half = gradX > gradY
                        ? glm::vec2(0.0f, 0.58f / static_cast<float>(resolution_))
                        : glm::vec2(0.58f / static_cast<float>(resolution_), 0.0f);
                    addSegment(TerrainFeatureType::ErosionEdge, faceIndex, center - half, center + half, erosionStrength);
                }
            }
        }
    }
}

void PlanetProceduralData::buildTerrainChunks(const PlanetRenderSettings& settings)
{
    PROFILE_SCOPE("Build Offline Terrain Chunks");
    terrainChunks_.clear();
    if (resolution_ <= 1) {
        return;
    }

    struct PendingNode {
        int faceIndex = 0;
        int depth = 0;
        glm::vec2 uvMin{0.0f, 0.0f};
        glm::vec2 uvSize{1.0f, 1.0f};
    };

    const auto sampleNodeStats = [&](const PendingNode& node,
                                     float& meshDensity,
                                     float& geometricError,
                                     float& featureMask,
                                     float& minHeight,
                                     float& maxHeight,
                                     bool& hasWater,
                                     bool& hasShore) {
        const FaceData& face = faces_[static_cast<std::size_t>(node.faceIndex)];
        meshDensity = 0.0f;
        geometricError = 0.0f;
        featureMask = 0.0f;
        minHeight = std::numeric_limits<float>::max();
        maxHeight = std::numeric_limits<float>::lowest();
        hasWater = false;
        hasShore = false;

        constexpr int kStatsSamples = 5;
        float densitySum = 0.0f;
        float errorSum = 0.0f;
        float featureSum = 0.0f;
        int sampleCount = 0;
        for (int y = 0; y < kStatsSamples; ++y) {
            for (int x = 0; x < kStatsSamples; ++x) {
                const glm::vec2 local(static_cast<float>(x) / static_cast<float>(kStatsSamples - 1),
                                      static_cast<float>(y) / static_cast<float>(kStatsSamples - 1));
                const glm::vec2 uv = node.uvMin + local * node.uvSize;
                const float height = sampleFaceLayerBilinear(face.height, resolution_, uv);
                const float density = sampleFaceLayerBilinear(face.meshDensity, resolution_, uv);
                const float error = sampleFaceLayerBilinear(face.geometricError, resolution_, uv);
                const float feature = sampleFaceLayerBilinear(face.featureMask, resolution_, uv);
                const float waterDepth = sampleFaceLayerBilinear(face.waterDepth, resolution_, uv);
                const float shore = sampleFaceLayerBilinear(face.shoreMask, resolution_, uv);

                densitySum += density;
                errorSum += error;
                featureSum += feature;
                meshDensity = glm::max(meshDensity, density);
                geometricError = glm::max(geometricError, error);
                featureMask = glm::max(featureMask, feature);
                minHeight = glm::min(minHeight, height);
                maxHeight = glm::max(maxHeight, height);
                hasWater = hasWater || waterDepth > 0.0001f;
                hasShore = hasShore || shore > 0.015f;
                ++sampleCount;
            }
        }

        if (sampleCount > 0) {
            meshDensity = glm::max(meshDensity, densitySum / static_cast<float>(sampleCount));
            geometricError = glm::max(geometricError, errorSum / static_cast<float>(sampleCount));
            featureMask = glm::max(featureMask, featureSum / static_cast<float>(sampleCount));
        }
    };

    const auto emitChunk = [&](const PendingNode& node,
                               float meshDensity,
                               float geometricError,
                               float featureMask,
                               float minHeight,
                               float maxHeight,
                               bool hasWater,
                               bool hasShore) {
        TerrainChunk chunk;
        chunk.faceIndex = node.faceIndex;
        chunk.depth = node.depth;
        chunk.uvMin = node.uvMin;
        chunk.uvSize = node.uvSize;
        chunk.minHeight = minHeight;
        chunk.maxHeight = maxHeight;
        chunk.geometricError = geometricError;
        chunk.meshDensity = meshDensity;
        chunk.featureMask = featureMask;
        chunk.hasWater = hasWater;
        chunk.hasShore = hasShore;

        const FaceData& face = faces_[static_cast<std::size_t>(node.faceIndex)];
        const int vertexSide = kTerrainChunkMeshResolution + 1;
        chunk.vertices.reserve(static_cast<std::size_t>(vertexSide * vertexSide));
        chunk.indices.reserve(static_cast<std::size_t>(kTerrainChunkMeshResolution * kTerrainChunkMeshResolution * 6));
        const bool useTinMesh = false;

        const auto sampleFeatureWeight = [&](const glm::vec2& uv) {
            const float channel = sampleFaceLayerBilinear(face.channelMask, resolution_, uv);
            const float flow = sampleFaceLayerBilinear(face.flowMask, resolution_, uv);
            const float shore = sampleFaceLayerBilinear(face.shoreMask, resolution_, uv);
            const float wear = sampleFaceLayerBilinear(face.wearMask, resolution_, uv);
            const float deposition = sampleFaceLayerBilinear(face.depositionMask, resolution_, uv);
            const float feature = sampleFaceLayerBilinear(face.featureMask, resolution_, uv);
            const glm::vec4 domain = sampleFaceVec4LayerBilinear(face.domainWeight, resolution_, uv);
            return glm::clamp(feature * 0.48f
                            + glm::max(channel, flow) * 0.84f
                            + shore * 0.58f
                            + glm::max(wear, deposition) * 0.48f
                            + domain.x * 0.24f
                            + domain.y * 0.36f
                            + domain.z * 0.28f,
                              0.0f,
                              1.0f);
        };

        const auto createVertex = [&](const glm::vec2& local) {
            const glm::vec2 uv = node.uvMin + local * node.uvSize;
            const glm::vec3 sphereDir = cubeSphereDirection(kFaces[static_cast<std::size_t>(node.faceIndex)], uv);
            const float height = sampleFaceLayerBilinear(face.height, resolution_, uv);
            const float normalStep = glm::max(1.0f / static_cast<float>(resolution_),
                                              node.uvSize.x / static_cast<float>(kTerrainChunkMeshResolution * 2));
            const glm::vec2 uvL(glm::max(uv.x - normalStep, 0.0f), uv.y);
            const glm::vec2 uvR(glm::min(uv.x + normalStep, 1.0f), uv.y);
            const glm::vec2 uvD(uv.x, glm::max(uv.y - normalStep, 0.0f));
            const glm::vec2 uvU(uv.x, glm::min(uv.y + normalStep, 1.0f));
            const glm::vec3 dirL = cubeSphereDirection(kFaces[static_cast<std::size_t>(node.faceIndex)], uvL);
            const glm::vec3 dirR = cubeSphereDirection(kFaces[static_cast<std::size_t>(node.faceIndex)], uvR);
            const glm::vec3 dirD = cubeSphereDirection(kFaces[static_cast<std::size_t>(node.faceIndex)], uvD);
            const glm::vec3 dirU = cubeSphereDirection(kFaces[static_cast<std::size_t>(node.faceIndex)], uvU);
            const float hL = sampleFaceLayerBilinear(face.height, resolution_, uvL);
            const float hR = sampleFaceLayerBilinear(face.height, resolution_, uvR);
            const float hD = sampleFaceLayerBilinear(face.height, resolution_, uvD);
            const float hU = sampleFaceLayerBilinear(face.height, resolution_, uvU);
            const glm::vec3 pL = dirL * (settings.planetRadius + hL * settings.terrainHeightScale);
            const glm::vec3 pR = dirR * (settings.planetRadius + hR * settings.terrainHeightScale);
            const glm::vec3 pD = dirD * (settings.planetRadius + hD * settings.terrainHeightScale);
            const glm::vec3 pU = dirU * (settings.planetRadius + hU * settings.terrainHeightScale);
            glm::vec3 normal = glm::normalize(glm::cross(pR - pL, pU - pD));
            if (glm::dot(normal, sphereDir) < 0.0f) {
                normal = -normal;
            }

            TerrainChunkVertex vertex;
            vertex.sphereDir = sphereDir;
            vertex.normal = normal;
            vertex.uv = uv;
            vertex.height = height;
            vertex.featureWeight = sampleFeatureWeight(uv);
            return vertex;
        };

        if (!useTinMesh) {
            for (int y = 0; y <= kTerrainChunkMeshResolution; ++y) {
                for (int x = 0; x <= kTerrainChunkMeshResolution; ++x) {
                    const glm::vec2 local(static_cast<float>(x) / static_cast<float>(kTerrainChunkMeshResolution),
                                          static_cast<float>(y) / static_cast<float>(kTerrainChunkMeshResolution));
                    chunk.vertices.push_back(createVertex(local));
                }
            }

            const auto gridVertexAt = [&](int x, int y) -> const TerrainChunkVertex& {
                return chunk.vertices[static_cast<std::size_t>(y * vertexSide + x)];
            };

            for (int y = 0; y < kTerrainChunkMeshResolution; ++y) {
                for (int x = 0; x < kTerrainChunkMeshResolution; ++x) {
                    const std::uint32_t bottomLeft = static_cast<std::uint32_t>(y * vertexSide + x);
                    const std::uint32_t bottomRight = bottomLeft + 1;
                    const std::uint32_t topLeft = static_cast<std::uint32_t>((y + 1) * vertexSide + x);
                    const std::uint32_t topRight = topLeft + 1;
                    const TerrainChunkVertex& bl = gridVertexAt(x, y);
                    const TerrainChunkVertex& br = gridVertexAt(x + 1, y);
                    const TerrainChunkVertex& tl = gridVertexAt(x, y + 1);
                    const TerrainChunkVertex& tr = gridVertexAt(x + 1, y + 1);
                    const float costA = glm::abs(bl.featureWeight - tr.featureWeight) * 1.65f
                                      + glm::abs(bl.height - tr.height) * 0.35f;
                    const float costB = glm::abs(br.featureWeight - tl.featureWeight) * 1.65f
                                      + glm::abs(br.height - tl.height) * 0.35f;

                    if (costA <= costB) {
                        chunk.indices.push_back(bottomLeft);
                        chunk.indices.push_back(bottomRight);
                        chunk.indices.push_back(topRight);
                        chunk.indices.push_back(bottomLeft);
                        chunk.indices.push_back(topRight);
                        chunk.indices.push_back(topLeft);
                    } else {
                        chunk.indices.push_back(bottomLeft);
                        chunk.indices.push_back(bottomRight);
                        chunk.indices.push_back(topLeft);
                        chunk.indices.push_back(bottomRight);
                        chunk.indices.push_back(topRight);
                        chunk.indices.push_back(topLeft);
                        ++chunk.flippedDiagonalCount;
                    }
                }
            }

            terrainChunks_.push_back(std::move(chunk));
            return;
        };

        std::vector<glm::vec2> localPoints;
        localPoints.reserve(96);
        const auto addLocalPoint = [&](const glm::vec2& local) {
            const glm::vec2 clamped = glm::clamp(local, glm::vec2(0.0f), glm::vec2(1.0f));
            for (std::size_t i = 0; i < localPoints.size(); ++i) {
                const glm::vec2& existing = localPoints[i];
                const glm::vec2 delta = existing - clamped;
                if (glm::dot(delta, delta) < 1.0e-8f) {
                    return static_cast<int>(i);
                }
            }
            localPoints.push_back(clamped);
            return static_cast<int>(localPoints.size() - 1);
        };

        struct ConstraintEdge {
            int a = 0;
            int b = 0;
        };
        std::vector<ConstraintEdge> constraintEdges;
        constraintEdges.reserve(32);

        for (int i = 0; i <= kTerrainChunkMeshResolution; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kTerrainChunkMeshResolution);
            addLocalPoint(glm::vec2(t, 0.0f));
            addLocalPoint(glm::vec2(t, 1.0f));
            addLocalPoint(glm::vec2(0.0f, t));
            addLocalPoint(glm::vec2(1.0f, t));
        }
        for (int y = 2; y < kTerrainChunkMeshResolution; y += 2) {
            for (int x = 2; x < kTerrainChunkMeshResolution; x += 2) {
                addLocalPoint(glm::vec2(static_cast<float>(x) / static_cast<float>(kTerrainChunkMeshResolution),
                                        static_cast<float>(y) / static_cast<float>(kTerrainChunkMeshResolution)));
            }
        }

        if (useTinMesh) {
            constexpr int kFeaturePointSamples = 6;
            for (int y = 0; y < kFeaturePointSamples; ++y) {
                for (int x = 0; x < kFeaturePointSamples; ++x) {
                    const glm::vec2 local((static_cast<float>(x) + 0.5f) / static_cast<float>(kFeaturePointSamples),
                                          (static_cast<float>(y) + 0.5f) / static_cast<float>(kFeaturePointSamples));
                    const glm::vec2 uv = node.uvMin + local * node.uvSize;
                    const float feature = sampleFeatureWeight(uv);
                    const float density = sampleFaceLayerBilinear(face.meshDensity, resolution_, uv);
                    const float error = sampleFaceLayerBilinear(face.geometricError, resolution_, uv);
                    const float score = glm::clamp(feature * 0.72f + density * 0.34f + error * 0.018f, 0.0f, 1.0f);
                    if (score < 0.46f) {
                        continue;
                    }

                    const float hash = glm::fract(std::sin(glm::dot(uv, glm::vec2(127.1f, 311.7f))) * 43758.5453f);
                    const float hashB = glm::fract(std::sin(glm::dot(uv, glm::vec2(269.5f, 183.3f))) * 24634.6345f);
                    const glm::vec2 jitter(hash - 0.5f, hashB - 0.5f);
                    const float jitterScale = 0.22f / static_cast<float>(kFeaturePointSamples);
                    addLocalPoint(glm::clamp(local + jitter * jitterScale, glm::vec2(0.02f), glm::vec2(0.98f)));
                }
            }
        }

        const auto clipSegmentToNode = [&](glm::vec2 start, glm::vec2 end, glm::vec2& clippedStart, glm::vec2& clippedEnd) {
            glm::vec2 delta = end - start;
            float t0 = 0.0f;
            float t1 = 1.0f;
            const auto clipAxis = [&](float p, float q) {
                if (glm::abs(p) < 1.0e-7f) {
                    return q >= 0.0f;
                }
                const float r = q / p;
                if (p < 0.0f) {
                    if (r > t1) {
                        return false;
                    }
                    t0 = glm::max(t0, r);
                } else {
                    if (r < t0) {
                        return false;
                    }
                    t1 = glm::min(t1, r);
                }
                return true;
            };

            if (!clipAxis(-delta.x, start.x - node.uvMin.x)
                || !clipAxis(delta.x, node.uvMin.x + node.uvSize.x - start.x)
                || !clipAxis(-delta.y, start.y - node.uvMin.y)
                || !clipAxis(delta.y, node.uvMin.y + node.uvSize.y - start.y)
                || t0 > t1) {
                return false;
            }

            clippedStart = start + delta * t0;
            clippedEnd = start + delta * t1;
            return glm::dot(clippedEnd - clippedStart, clippedEnd - clippedStart) > 1.0e-10f;
        };

        const auto uvToLocal = [&](const glm::vec2& uv) {
            return (uv - node.uvMin) / node.uvSize;
        };

        for (const TerrainFeatureSegment& segment : terrainFeatureSegments_) {
            if (constraintEdges.size() >= 12) {
                break;
            }
            if (segment.faceIndex != node.faceIndex || segment.strength < 0.34f) {
                continue;
            }

            glm::vec2 clippedA;
            glm::vec2 clippedB;
            if (!clipSegmentToNode(segment.uvA, segment.uvB, clippedA, clippedB)) {
                continue;
            }

            const float localLength = glm::length(uvToLocal(clippedB) - uvToLocal(clippedA));
            const int subdivisions = glm::clamp(static_cast<int>(std::ceil(localLength * 5.0f)), 1, 4);
            int previousIndex = -1;
            for (int i = 0; i <= subdivisions; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(subdivisions);
                const int pointIndex = addLocalPoint(uvToLocal(glm::mix(clippedA, clippedB, t)));
                if (previousIndex >= 0 && previousIndex != pointIndex) {
                    constraintEdges.push_back(ConstraintEdge{previousIndex, pointIndex});
                    if (constraintEdges.size() >= 12) {
                        break;
                    }
                }
                previousIndex = pointIndex;
            }
        }
        chunk.constrainedEdgeCount = static_cast<int>(constraintEdges.size());

        for (const glm::vec2& local : localPoints) {
            chunk.vertices.push_back(createVertex(local));
        }

        struct TinTriangle {
            int a = 0;
            int b = 0;
            int c = 0;
        };
        struct TinEdge {
            int a = 0;
            int b = 0;
        };

        std::vector<glm::vec2> triangulationPoints = localPoints;
        const int originalPointCount = static_cast<int>(triangulationPoints.size());
        triangulationPoints.push_back(glm::vec2(-8.0f, -8.0f));
        triangulationPoints.push_back(glm::vec2(9.0f, -8.0f));
        triangulationPoints.push_back(glm::vec2(0.5f, 9.0f));

        const auto orient2d = [&](int a, int b, int c) {
            const glm::vec2 ab = triangulationPoints[static_cast<std::size_t>(b)] - triangulationPoints[static_cast<std::size_t>(a)];
            const glm::vec2 ac = triangulationPoints[static_cast<std::size_t>(c)] - triangulationPoints[static_cast<std::size_t>(a)];
            return ab.x * ac.y - ab.y * ac.x;
        };

        const auto circumcircleContains = [&](const TinTriangle& triangle, const glm::vec2& point) {
            const glm::vec2 a = triangulationPoints[static_cast<std::size_t>(triangle.a)];
            const glm::vec2 b = triangulationPoints[static_cast<std::size_t>(triangle.b)];
            const glm::vec2 c = triangulationPoints[static_cast<std::size_t>(triangle.c)];
            const float d = 2.0f * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
            if (glm::abs(d) < 1.0e-8f) {
                return false;
            }
            const float aa = glm::dot(a, a);
            const float bb = glm::dot(b, b);
            const float cc = glm::dot(c, c);
            const glm::vec2 center(
                (aa * (b.y - c.y) + bb * (c.y - a.y) + cc * (a.y - b.y)) / d,
                (aa * (c.x - b.x) + bb * (a.x - c.x) + cc * (b.x - a.x)) / d
            );
            const float radiusSq = glm::dot(center - a, center - a);
            const float distanceSq = glm::dot(center - point, center - point);
            return distanceSq <= radiusSq + 1.0e-7f;
        };

        std::vector<TinTriangle> triangles;
        triangles.push_back(TinTriangle{originalPointCount, originalPointCount + 1, originalPointCount + 2});
        if (orient2d(triangles.front().a, triangles.front().b, triangles.front().c) < 0.0f) {
            std::swap(triangles.front().b, triangles.front().c);
        }

        for (int pointIndex = 0; pointIndex < originalPointCount; ++pointIndex) {
            std::vector<TinEdge> boundary;
            std::vector<TinTriangle> kept;
            kept.reserve(triangles.size() + 2);

            const auto addBoundaryEdge = [&](TinEdge edge) {
                for (auto it = boundary.begin(); it != boundary.end(); ++it) {
                    if (it->a == edge.b && it->b == edge.a) {
                        boundary.erase(it);
                        return;
                    }
                }
                boundary.push_back(edge);
            };

            for (const TinTriangle& triangle : triangles) {
                if (circumcircleContains(triangle, triangulationPoints[static_cast<std::size_t>(pointIndex)])) {
                    addBoundaryEdge(TinEdge{triangle.a, triangle.b});
                    addBoundaryEdge(TinEdge{triangle.b, triangle.c});
                    addBoundaryEdge(TinEdge{triangle.c, triangle.a});
                } else {
                    kept.push_back(triangle);
                }
            }

            for (TinEdge edge : boundary) {
                TinTriangle triangle{edge.a, edge.b, pointIndex};
                if (orient2d(triangle.a, triangle.b, triangle.c) < 0.0f) {
                    std::swap(triangle.a, triangle.b);
                }
                kept.push_back(triangle);
            }
            triangles = std::move(kept);
        }

        const auto triangleHasEdge = [](const TinTriangle& triangle, int a, int b) {
            return ((triangle.a == a && triangle.b == b) || (triangle.a == b && triangle.b == a))
                || ((triangle.b == a && triangle.c == b) || (triangle.b == b && triangle.c == a))
                || ((triangle.c == a && triangle.a == b) || (triangle.c == b && triangle.a == a));
        };

        const auto edgeCrossesConstraint = [&](int edgeA, int edgeB, int constraintA, int constraintB) {
            if (edgeA == constraintA || edgeA == constraintB || edgeB == constraintA || edgeB == constraintB) {
                return false;
            }
            const float o1 = orient2d(constraintA, constraintB, edgeA);
            const float o2 = orient2d(constraintA, constraintB, edgeB);
            const float o3 = orient2d(edgeA, edgeB, constraintA);
            const float o4 = orient2d(edgeA, edgeB, constraintB);
            return o1 * o2 < -1.0e-8f && o3 * o4 < -1.0e-8f;
        };

        const auto tryRecoverConstraint = [&](const ConstraintEdge& constraint) {
            if (constraint.a == constraint.b) {
                return false;
            }

            constexpr int kMaxRecoveryIterations = 48;
            for (int iteration = 0; iteration < kMaxRecoveryIterations; ++iteration) {
                bool alreadyRecovered = false;
                for (const TinTriangle& triangle : triangles) {
                    if (triangleHasEdge(triangle, constraint.a, constraint.b)) {
                        alreadyRecovered = true;
                        break;
                    }
                }
                if (alreadyRecovered) {
                    return true;
                }

                bool flipped = false;
                for (std::size_t firstIndex = 0; firstIndex < triangles.size() && !flipped; ++firstIndex) {
                    const TinTriangle& first = triangles[firstIndex];
                    const int firstVertices[3] = { first.a, first.b, first.c };
                    for (int edgeIndex = 0; edgeIndex < 3 && !flipped; ++edgeIndex) {
                        const int edgeA = firstVertices[edgeIndex];
                        const int edgeB = firstVertices[(edgeIndex + 1) % 3];
                        if (!edgeCrossesConstraint(edgeA, edgeB, constraint.a, constraint.b)) {
                            continue;
                        }

                        for (std::size_t secondIndex = firstIndex + 1; secondIndex < triangles.size(); ++secondIndex) {
                            const TinTriangle& second = triangles[secondIndex];
                            if (!triangleHasEdge(second, edgeA, edgeB)) {
                                continue;
                            }

                            const int firstOpposite = firstVertices[(edgeIndex + 2) % 3];
                            int secondOpposite = -1;
                            const int secondVertices[3] = { second.a, second.b, second.c };
                            for (int vertex : secondVertices) {
                                if (vertex != edgeA && vertex != edgeB) {
                                    secondOpposite = vertex;
                                    break;
                                }
                            }
                            if (secondOpposite < 0 || firstOpposite == secondOpposite) {
                                continue;
                            }

                            if (orient2d(firstOpposite, secondOpposite, edgeA) * orient2d(firstOpposite, secondOpposite, edgeB) >= -1.0e-8f) {
                                continue;
                            }

                            TinTriangle replacementA{firstOpposite, secondOpposite, edgeA};
                            TinTriangle replacementB{secondOpposite, firstOpposite, edgeB};
                            if (orient2d(replacementA.a, replacementA.b, replacementA.c) < 0.0f) {
                                std::swap(replacementA.a, replacementA.b);
                            }
                            if (orient2d(replacementB.a, replacementB.b, replacementB.c) < 0.0f) {
                                std::swap(replacementB.a, replacementB.b);
                            }
                            if (orient2d(replacementA.a, replacementA.b, replacementA.c) <= 1.0e-8f
                                || orient2d(replacementB.a, replacementB.b, replacementB.c) <= 1.0e-8f) {
                                continue;
                            }

                            triangles[firstIndex] = replacementA;
                            triangles[secondIndex] = replacementB;
                            flipped = true;
                            break;
                        }
                    }
                }

                if (!flipped) {
                    return false;
                }
            }
            return false;
        };

        for (const ConstraintEdge& edge : constraintEdges) {
            if (tryRecoverConstraint(edge)) {
                ++chunk.recoveredConstraintEdgeCount;
            }
        }

        for (const TinTriangle& triangle : triangles) {
            if (triangle.a >= originalPointCount || triangle.b >= originalPointCount || triangle.c >= originalPointCount) {
                continue;
            }
            if (orient2d(triangle.a, triangle.b, triangle.c) <= 1.0e-8f) {
                continue;
            }
            chunk.indices.push_back(static_cast<std::uint32_t>(triangle.a));
            chunk.indices.push_back(static_cast<std::uint32_t>(triangle.b));
            chunk.indices.push_back(static_cast<std::uint32_t>(triangle.c));
        }

        terrainChunks_.push_back(std::move(chunk));
    };

    std::vector<PendingNode> stack;
    stack.reserve(512);
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        stack.push_back(PendingNode{faceIndex, 0, glm::vec2(0.0f), glm::vec2(1.0f)});
    }

    while (!stack.empty()) {
        const PendingNode node = stack.back();
        stack.pop_back();

        float meshDensity = 0.0f;
        float geometricError = 0.0f;
        float featureMask = 0.0f;
        float minHeight = 0.0f;
        float maxHeight = 0.0f;
        bool hasWater = false;
        bool hasShore = false;
        sampleNodeStats(node, meshDensity, geometricError, featureMask, minHeight, maxHeight, hasWater, hasShore);

        const float splitScore = meshDensity * 0.55f
                               + geometricError * 1.35f
                               + featureMask * 0.75f
                               + (hasShore ? 0.35f : 0.0f);
        const float depthBias = static_cast<float>(node.depth) * 0.13f;
        const bool forceBase = node.depth < kTerrainChunkMinDepth;
        const bool adaptiveSplit = splitScore > (0.86f + depthBias);
        if ((forceBase || adaptiveSplit) && node.depth < kTerrainChunkMaxDepth) {
            const glm::vec2 childSize = node.uvSize * 0.5f;
            for (int childY = 0; childY < 2; ++childY) {
                for (int childX = 0; childX < 2; ++childX) {
                    PendingNode child;
                    child.faceIndex = node.faceIndex;
                    child.depth = node.depth + 1;
                    child.uvSize = childSize;
                    child.uvMin = node.uvMin + glm::vec2(static_cast<float>(childX), static_cast<float>(childY)) * childSize;
                    stack.push_back(child);
                }
            }
            continue;
        }

        emitChunk(node, meshDensity, geometricError, featureMask, minHeight, maxHeight, hasWater, hasShore);
    }
}

void PlanetProceduralData::buildTerrainSkeletons(const PlanetRenderSettings& settings)
{
    PROFILE_SCOPE("Build Terrain Skeletons");
    terrainSkeletons_.clear();

    const auto addSkeleton = [&](TerrainSkeletonType type,
                                 int faceIndex,
                                 const glm::vec2& uvA,
                                 const glm::vec2& uvB,
                                 float width,
                                 float strength,
                                 float falloff,
                                 float variation = 0.0f) {
        TerrainSkeletonSegment segment;
        segment.type = type;
        segment.faceIndex = faceIndex;
        segment.uvA = glm::clamp(uvA, glm::vec2(0.02f), glm::vec2(0.98f));
        segment.uvB = glm::clamp(uvB, glm::vec2(0.02f), glm::vec2(0.98f));
        segment.sphereDirA = cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], segment.uvA);
        segment.sphereDirB = cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], segment.uvB);
        segment.width = glm::max(width, 0.001f);
        segment.strength = strength;
        segment.falloff = glm::max(falloff, 0.1f);
        segment.variation = variation;
        terrainSkeletons_.push_back(segment);
    };

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const glm::vec3 seedOffset(
            static_cast<float>(faceIndex) * 17.3f + 4.1f,
            static_cast<float>(faceIndex) * 9.7f + 11.4f,
            static_cast<float>(faceIndex) * 13.1f + 2.8f
        );

        for (int beltIndex = 0; beltIndex < 3; ++beltIndex) {
            const float bandSeed = static_cast<float>(beltIndex);
            const float n0 = fbm(seedOffset + glm::vec3(1.7f + bandSeed, 5.2f, 9.3f), 3, 2.0f, 0.5f) * 0.5f + 0.5f;
            const float n1 = fbm(seedOffset + glm::vec3(7.4f, 2.6f + bandSeed, 3.8f), 3, 2.0f, 0.5f) * 0.5f + 0.5f;
            const float n2 = fbm(seedOffset + glm::vec3(4.9f, 12.1f, 6.5f + bandSeed), 3, 2.0f, 0.5f) * 0.5f + 0.5f;
            const float rangeType = fbm(seedOffset + glm::vec3(10.9f, 8.2f + bandSeed, 1.6f), 3, 2.0f, 0.5f) * 0.5f + 0.5f;
            const bool broadMassif = rangeType > 0.58f;
            const float angle = glm::mix(-0.95f, 0.95f, n0);
            const glm::vec2 center(glm::mix(0.28f, 0.72f, n1), glm::mix(0.28f, 0.72f, n2));
            const glm::vec2 axis = glm::normalize(glm::vec2(std::cos(angle), std::sin(angle)));
            const glm::vec2 normal(-axis.y, axis.x);
            const float halfLength = glm::mix(0.28f, 0.48f, n2);
            const float width = glm::mix(broadMassif ? 0.185f : 0.145f, broadMassif ? 0.260f : 0.205f, n0);
            const float strength = settings.mountainMaskStrength * glm::mix(broadMassif ? 0.050f : 0.058f, broadMassif ? 0.078f : 0.090f, n1);
            const glm::vec2 bend = normal * glm::mix(-0.18f, 0.18f, fbm(seedOffset + glm::vec3(15.1f, 3.4f, bandSeed), 3, 2.0f, 0.5f) * 0.5f + 0.5f);
            const glm::vec2 beltA = center - axis * halfLength;
            const glm::vec2 beltB = center + axis * halfLength;
            const glm::vec2 beltMid = center + bend;
            const glm::vec2 beltA0 = glm::mix(beltA, beltMid, 0.52f);
            const glm::vec2 beltB0 = glm::mix(beltMid, beltB, 0.52f);

            addSkeleton(TerrainSkeletonType::Massif, faceIndex, beltMid - normal * width * 0.28f, beltMid + normal * width * 0.28f, width * (broadMassif ? 1.55f : 1.28f), strength * (broadMassif ? 0.82f : 0.62f), broadMassif ? 1.18f : 1.32f, rangeType);
            addSkeleton(TerrainSkeletonType::MountainBelt, faceIndex, beltA, beltA0, width, strength * 0.78f, 1.12f, rangeType);
            addSkeleton(TerrainSkeletonType::MountainBelt, faceIndex, beltA0, beltB0, width * 1.16f, strength * 0.92f, 1.05f, rangeType);
            addSkeleton(TerrainSkeletonType::MountainBelt, faceIndex, beltB0, beltB, width, strength * 0.78f, 1.12f, rangeType);
            if (!broadMassif) {
                addSkeleton(TerrainSkeletonType::Ridge, faceIndex, beltA + normal * width * 0.12f, beltMid + normal * width * 0.22f, width * 0.34f, strength * 0.46f, 1.75f, rangeType);
                addSkeleton(TerrainSkeletonType::Ridge, faceIndex, beltMid + normal * width * 0.22f, beltB + normal * width * 0.10f, width * 0.34f, strength * 0.46f, 1.75f, rangeType);
            }
            addSkeleton(TerrainSkeletonType::Valley, faceIndex, beltA0 - normal * width * 0.82f, beltMid - normal * width * 0.48f, width * 0.58f, strength * 0.24f, 1.28f, rangeType);
            addSkeleton(TerrainSkeletonType::Valley, faceIndex, beltMid + normal * width * 0.50f, beltB0 + normal * width * 0.86f, width * 0.58f, strength * 0.22f, 1.28f, rangeType);

            const glm::vec2 branchAnchor = glm::mix(beltA0, beltB0, glm::mix(0.28f, 0.72f, n0));
            const glm::vec2 branchDir = glm::normalize(axis * glm::mix(-0.35f, 0.35f, n1) + normal * (n2 > 0.5f ? 1.0f : -1.0f));
            addSkeleton(TerrainSkeletonType::Ridge,
                        faceIndex,
                        branchAnchor,
                        branchAnchor + branchDir * halfLength * 0.42f,
                        width * (broadMassif ? 0.30f : 0.25f),
                        strength * (broadMassif ? 0.22f : 0.32f),
                        broadMassif ? 1.45f : 1.65f,
                        rangeType);
            addSkeleton(TerrainSkeletonType::Valley,
                        faceIndex,
                        branchAnchor - branchDir * halfLength * 0.10f,
                        branchAnchor - branchDir * halfLength * 0.36f + normal * width * (n2 > 0.5f ? -0.45f : 0.45f),
                        width * 0.46f,
                        strength * 0.16f,
                        1.20f,
                        rangeType);
        }
    }
}

void PlanetProceduralData::buildTerrainPeakNodes(const PlanetRenderSettings& settings)
{
    PROFILE_SCOPE("Build Terrain Peak Nodes");
    terrainPeakNodes_.clear();
    if (terrainSkeletons_.empty()) {
        return;
    }

    const auto addPeak = [&](TerrainPeakType type,
                             int faceIndex,
                             const glm::vec2& uv,
                             float radius,
                             float height,
                             float sharpness,
                             float variation) {
        TerrainPeakNode node;
        node.type = type;
        node.faceIndex = faceIndex;
        node.uv = glm::clamp(uv, glm::vec2(0.02f), glm::vec2(0.98f));
        node.sphereDir = cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], node.uv);
        node.radius = glm::max(radius, 0.001f);
        node.height = height;
        node.sharpness = glm::max(sharpness, 0.1f);
        node.variation = variation;
        terrainPeakNodes_.push_back(node);
    };

    for (const TerrainSkeletonSegment& skeleton : terrainSkeletons_) {
        if (skeleton.type == TerrainSkeletonType::Valley) {
            continue;
        }

        const glm::vec2 segment = skeleton.uvB - skeleton.uvA;
        const float length = glm::length(segment);
        if (length <= 0.001f) {
            continue;
        }

        const int peakCount = skeleton.type == TerrainSkeletonType::Massif
            ? 1
            : (skeleton.type == TerrainSkeletonType::Ridge ? 2 : 2);
        for (int i = 0; i < peakCount; ++i) {
            const float tBase = (static_cast<float>(i) + 0.5f) / static_cast<float>(peakCount);
            const float tNoise = perlinNoise(glm::vec3(
                skeleton.variation * 7.1f + static_cast<float>(i) * 2.3f,
                static_cast<float>(skeleton.faceIndex) * 3.7f,
                length * 11.4f
            )) * 0.5f + 0.5f;
            const float t = glm::clamp(tBase + (tNoise - 0.5f) * 0.18f, 0.08f, 0.92f);
            const glm::vec2 axis = glm::normalize(segment);
            const glm::vec2 normal(-axis.y, axis.x);
            const float sideNoise = perlinNoise(glm::vec3(
                skeleton.variation * 5.3f,
                static_cast<float>(i) * 4.9f,
                static_cast<float>(skeleton.faceIndex) * 2.8f
            ));
            const glm::vec2 uv = glm::mix(skeleton.uvA, skeleton.uvB, t) + normal * skeleton.width * sideNoise * 0.24f;
            const float sizeNoise = perlinNoise(glm::vec3(
                skeleton.variation * 8.6f + static_cast<float>(i),
                13.2f,
                static_cast<float>(skeleton.faceIndex) * 4.1f
            )) * 0.5f + 0.5f;
            const bool volcanic = skeleton.type == TerrainSkeletonType::Massif && sizeNoise > 0.82f;
            const TerrainPeakType type = volcanic
                ? TerrainPeakType::Volcanic
                : (skeleton.type == TerrainSkeletonType::Ridge ? TerrainPeakType::RidgePeak : TerrainPeakType::Massif);
            const float radiusScale = type == TerrainPeakType::Massif ? 0.54f : (type == TerrainPeakType::RidgePeak ? 0.30f : 0.38f);
            const float heightScale = type == TerrainPeakType::Volcanic ? 1.16f : (type == TerrainPeakType::RidgePeak ? 0.92f : 1.0f);
            addPeak(
                type,
                skeleton.faceIndex,
                uv,
                skeleton.width * glm::mix(radiusScale * 0.78f, radiusScale * 1.28f, sizeNoise),
                settings.mountainMaskStrength * glm::mix(0.078f, 0.138f, sizeNoise) * heightScale,
                type == TerrainPeakType::Massif ? 1.30f : (type == TerrainPeakType::RidgePeak ? 1.85f : 1.55f),
                sizeNoise
            );
        }
    }
}

void PlanetProceduralData::applyTerrainSkeletons(const PlanetRenderSettings& settings,
                                                 const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Apply Terrain Skeletons");
    if (terrainSkeletons_.empty() || resolution_ <= 1) {
        return;
    }

    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FaceData& faceData = faces_[faceIndex];
        for (int y = 0; y < resolution_; ++y) {
            for (int x = 0; x < resolution_; ++x) {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution_),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution_)
                );
                const glm::vec3 sphereDir = cubeSphereDirection(kFaces[faceIndex], uv);
                const std::size_t index = static_cast<std::size_t>(y * resolution_ + x);
                float height = faceData.height[index];
                const float landMask = glm::smoothstep(settings.seaLevelOffset - 0.08f, settings.seaLevelOffset + 0.10f, height);

                for (const TerrainSkeletonSegment& skeleton : terrainSkeletons_) {
                    if (skeleton.faceIndex != static_cast<int>(faceIndex)) {
                        continue;
                    }

                    float along = 0.0f;
                    const float distance = distanceToSegmentOnSphere(sphereDir, skeleton.sphereDirA, skeleton.sphereDirB, along);
                    const float angularWidth = glm::max(skeleton.width * 1.57079637f, 0.001f);
                    const float normalizedDistance = distance / angularWidth;
                    if (normalizedDistance >= 1.0f) {
                        continue;
                    }

                    const float axialFade = glm::smoothstep(0.0f, 0.22f, along) * (1.0f - glm::smoothstep(0.78f, 1.0f, along));
                    const float radialFalloff = std::exp(-normalizedDistance * normalizedDistance * 2.85f);
                    const float softEdge = 1.0f - glm::smoothstep(0.68f, 1.0f, normalizedDistance);
                    const float lateral = std::pow(glm::clamp(radialFalloff * softEdge, 0.0f, 1.0f), skeleton.falloff);
                    const float lowDetail = perlinNoise(sphereDir * 3.2f + glm::vec3(12.5f, 4.7f, 21.6f)) * 0.5f + 0.5f;
                    const float slopeDetail = perlinNoise(sphereDir * 5.6f + glm::vec3(2.6f, 18.4f, 7.2f)) * 0.5f + 0.5f;
                    const float terraceNoise = perlinNoise(sphereDir * 7.5f + glm::vec3(8.8f, 3.1f, 14.6f));
                    const float structuralNoise = glm::mix(lowDetail, slopeDetail, glm::smoothstep(0.0f, 0.58f, 1.0f - skeleton.variation));
                    const float influence = lateral * axialFade * landMask;
                    const float slopeBand = glm::smoothstep(0.18f, 0.42f, normalizedDistance)
                                          * (1.0f - glm::smoothstep(0.72f, 0.98f, normalizedDistance));

                    switch (skeleton.type) {
                    case TerrainSkeletonType::Massif: {
                        const float dome = std::exp(-normalizedDistance * normalizedDistance * glm::mix(1.65f, 2.55f, 1.0f - skeleton.variation));
                        const float uplift = skeleton.strength * influence * (0.18f + lowDetail * 0.025f) * dome;
                        height += uplift;
                        height += skeleton.strength * influence * slopeBand * terraceNoise * 0.0015f;
                        break;
                    }
                    case TerrainSkeletonType::MountainBelt: {
                        const float flank = std::exp(-normalizedDistance * normalizedDistance * 2.05f);
                        const float uplift = skeleton.strength * influence * (0.16f + structuralNoise * 0.025f) * flank;
                        height += uplift;
                        height += skeleton.strength * influence * slopeBand * terraceNoise * 0.0012f;
                        break;
                    }
                    case TerrainSkeletonType::Ridge: {
                        const float ridgeCore = std::pow(glm::max(1.0f - normalizedDistance, 0.0f), 1.75f);
                        height += skeleton.strength * influence * (0.20f + structuralNoise * 0.025f) * ridgeCore;
                        break;
                    }
                    case TerrainSkeletonType::Valley: {
                        const float valleyCore = std::pow(glm::max(1.0f - normalizedDistance, 0.0f), 1.12f);
                        const float valleyFloor = 1.0f - glm::smoothstep(0.0f, 0.42f, normalizedDistance);
                        height -= skeleton.strength * influence * (0.18f + structuralNoise * 0.020f) * valleyCore;
                        height = glm::mix(height, height - skeleton.strength * 0.035f * influence, valleyFloor * 0.22f);
                        faceData.flowMask[index] = glm::max(faceData.flowMask[index], influence * 0.55f);
                        faceData.erosionMask[index] = glm::max(faceData.erosionMask[index], influence * 0.35f);
                        break;
                    }
                    }
                }

                faceData.height[index] = height;
            }
            if (advanceProgress) {
                advanceProgress("Applying terrain skeleton shaping");
            }
        }
    }
}

void PlanetProceduralData::applyHeightfieldNoiseLayers(const PlanetRenderSettings& settings,
                                                       const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Apply Heightfield Noise Layers");
    if (resolution_ <= 1) {
        return;
    }

    const float seaLevel = settings.seaLevelOffset;
    const float heightScale = glm::max(settings.terrainHeightScale, 0.001f);
    const float amplitudeScale = glm::clamp(heightScale / 22.0f, 0.60f, 1.35f);

    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FaceData& faceData = faces_[faceIndex];
        for (int y = 0; y < resolution_; ++y) {
            for (int x = 0; x < resolution_; ++x) {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution_),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution_)
                );
                const glm::vec3 sphereDir = cubeSphereDirection(kFaces[faceIndex], uv);
                const std::size_t index = static_cast<std::size_t>(y * resolution_ + x);
                float height = faceData.height[index];
                const float relativeHeight = height - seaLevel;
                const float landMask = glm::smoothstep(0.025f, 0.20f, relativeHeight);
                if (landMask <= 0.001f) {
                    continue;
                }

                const glm::vec3 p = sphereDir * settings.terrainNoiseScale;
                const glm::vec3 warp(
                    perlinNoise(p * 0.18f + glm::vec3(31.1f, 4.2f, 11.8f)),
                    perlinNoise(p * 0.18f + glm::vec3(9.6f, 27.4f, 3.2f)),
                    perlinNoise(p * 0.18f + glm::vec3(5.1f, 8.7f, 24.6f))
                );

                float skeletonMountain = 0.0f;
                float skeletonRidge = 0.0f;
                float skeletonValley = 0.0f;
                float massifWeight = 0.0f;
                float skeletonShoulder = 0.0f;
                float skeletonCore = 0.0f;
                const bool useSkeletonHeightControl = false;
                if (useSkeletonHeightControl) for (const TerrainSkeletonSegment& skeleton : terrainSkeletons_) {
                    if (skeleton.faceIndex != static_cast<int>(faceIndex)) {
                        continue;
                    }

                    float along = 0.0f;
                    const float distance = distanceToSegmentOnSphere(sphereDir, skeleton.sphereDirA, skeleton.sphereDirB, along);
                    const float angularWidth = glm::max(skeleton.width * 1.57079637f, 0.001f);
                    const float normalizedDistance = distance / angularWidth;
                    if (normalizedDistance >= 1.0f) {
                        continue;
                    }

                    const float axialFade = glm::smoothstep(0.0f, 0.22f, along)
                                          * (1.0f - glm::smoothstep(0.78f, 1.0f, along));
                    const float radialFalloff = std::exp(-normalizedDistance * normalizedDistance * 2.65f);
                    const float softEdge = 1.0f - glm::smoothstep(0.66f, 1.0f, normalizedDistance);
                    const float lateral = std::pow(glm::clamp(radialFalloff * softEdge, 0.0f, 1.0f), skeleton.falloff);
                    const float influence = lateral * axialFade * landMask;
                    const float coreScale = skeleton.type == TerrainSkeletonType::Massif
                        ? 0.38f
                        : (skeleton.type == TerrainSkeletonType::MountainBelt ? 0.26f : 0.18f);
                    const float coreDistance = normalizedDistance / glm::max(coreScale, 0.001f);
                    const float longitudinalNoise = perlinNoise(
                        sphereDir * 4.20f + glm::vec3(along * 5.7f, skeleton.variation * 6.3f, 18.4f)
                    ) * 0.5f + 0.5f;
                    const float peakWaveA = std::sin((along + skeleton.variation * 0.37f) * 18.849556f);
                    const float peakWaveB = std::sin((along * 1.73f + skeleton.variation * 0.19f) * 18.849556f);
                    const float peakTrain = std::pow(glm::clamp(peakWaveA * 0.5f + 0.5f, 0.0f, 1.0f), 2.4f)
                                          * glm::mix(0.55f, 1.0f, peakWaveB * 0.5f + 0.5f);
                    const float peakGate = glm::smoothstep(0.46f, 0.82f, longitudinalNoise * 0.58f + peakTrain * 0.42f);
                    const float brokenAlong = glm::mix(0.10f, 1.08f, peakGate);
                    const float coreProfile = std::exp(-coreDistance * coreDistance * 2.65f) * axialFade * landMask * brokenAlong;
                    skeletonShoulder = 1.0f - (1.0f - skeletonShoulder) * (1.0f - influence * 0.22f);
                    skeletonCore = 1.0f - (1.0f - skeletonCore) * (1.0f - coreProfile * 0.92f);
                    if (skeleton.type == TerrainSkeletonType::Valley) {
                        skeletonValley = 1.0f - (1.0f - skeletonValley) * (1.0f - influence * 0.75f);
                    } else if (skeleton.type == TerrainSkeletonType::Ridge) {
                        skeletonRidge = 1.0f - (1.0f - skeletonRidge) * (1.0f - coreProfile * 0.92f);
                        skeletonMountain = 1.0f - (1.0f - skeletonMountain) * (1.0f - influence * 0.18f);
                    } else {
                        skeletonMountain = 1.0f - (1.0f - skeletonMountain) * (1.0f - influence * 0.24f);
                        const float massifContribution = coreProfile * (skeleton.type == TerrainSkeletonType::Massif ? 0.95f : 0.32f);
                        massifWeight = 1.0f - (1.0f - massifWeight) * (1.0f - massifContribution);
                    }
                }

                const glm::vec3 terrainP = p + warp * 0.30f;
                const float provinceNoise = perlinNoise(terrainP * 0.34f + glm::vec3(13.7f, 2.1f, 19.4f)) * 0.5f + 0.5f;
                const float hillNoise = perlinNoise(terrainP * 0.72f + glm::vec3(7.2f, 18.4f, 2.6f)) * 0.5f + 0.5f;
                const float volcanicNoise = perlinNoise(terrainP * 1.05f + glm::vec3(28.4f, 6.3f, 15.8f)) * 0.5f + 0.5f;
                const float stableInterior = glm::smoothstep(0.07f, 0.34f, relativeHeight);
                const float mountainProvince = glm::smoothstep(0.58f, 0.82f, provinceNoise) * stableInterior;
                const float hillDomain = glm::smoothstep(0.36f, 0.72f, hillNoise) * stableInterior;
                const float volcanicDomain = glm::smoothstep(0.82f, 0.96f, volcanicNoise) * stableInterior;
                const float mountainDomain = glm::clamp(mountainProvince * 0.28f + volcanicDomain * 0.18f, 0.0f, 1.0f);

                const float ridgeShape = std::pow(glm::clamp(glm::max(skeletonRidge, skeletonCore * 0.82f), 0.0f, 1.0f), 1.35f);
                const float massifShape = std::pow(glm::clamp(glm::max(massifWeight, mountainProvince * 0.18f), 0.0f, 1.0f), 1.25f);
                const float coneShape = std::pow(glm::clamp(volcanicDomain, 0.0f, 1.0f), 1.90f);
                const float detail = perlinNoise(terrainP * 1.45f + glm::vec3(4.6f, 11.2f, 23.7f));
                const float gentleWrinkle = perlinNoise(terrainP * 2.20f + glm::vec3(16.5f, 3.2f, 8.4f));

                const float hillUplift = hillDomain * (0.010f + hillNoise * 0.012f);
                const float mountainUplift = settings.mountainMaskStrength * amplitudeScale
                    * (skeletonShoulder * 0.0015f + skeletonCore * 0.060f + massifShape * 0.032f + ridgeShape * 0.040f);
                const float volcanicUplift = settings.mountainMaskStrength * amplitudeScale
                    * coneShape * (0.095f + volcanicNoise * 0.045f);
                const float valleyCarve = 0.0f;
                const float terrainDetail = (detail * glm::max(skeletonCore, mountainProvince * 0.55f) * 0.0055f + gentleWrinkle * hillDomain * 0.0025f)
                                          * (1.0f - skeletonValley * 0.65f);

                float peakMask = 0.0f;
                float peakCore = 0.0f;
                float peakMaterial = 0.0f;
                for (const TerrainPeakNode& peak : terrainPeakNodes_) {
                    const float angularDistance = std::acos(glm::clamp(glm::dot(sphereDir, peak.sphereDir), -1.0f, 1.0f));
                    const float angularRadius = glm::max(peak.radius * 1.57079637f, 0.001f);
                    const float boundaryNoise = perlinNoise(
                        sphereDir * 5.2f + glm::vec3(peak.variation * 12.7f, 6.4f, 21.3f)
                    ) * 0.5f + 0.5f;
                    const float effectiveRadius = angularRadius * glm::mix(0.78f, 1.24f, boundaryNoise);
                    const float d = angularDistance / glm::max(effectiveRadius, 0.001f);
                    if (d >= 1.65f) {
                        continue;
                    }

                    const float footprint = std::exp(-d * d * peak.sharpness);
                    const float core = std::pow(glm::clamp(1.0f - d * d, 0.0f, 1.0f), peak.type == TerrainPeakType::RidgePeak ? 1.85f : 1.35f);
                    const float peakNoise = perlinNoise(sphereDir * 7.2f + glm::vec3(peak.variation * 9.1f, 3.4f, 12.7f)) * 0.5f + 0.5f;
                    const float shoulder = std::exp(-d * d * 0.90f)
                                         * glm::smoothstep(0.16f, 0.72f, boundaryNoise * 0.58f + footprint * 0.42f);

                    peakMask = 1.0f - (1.0f - peakMask) * (1.0f - shoulder * 0.72f);
                    peakCore = 1.0f - (1.0f - peakCore) * (1.0f - core * glm::mix(0.30f, 0.52f, peakNoise));
                    peakMaterial = glm::max(peakMaterial, glm::clamp(footprint * (0.24f + core * 0.36f), 0.0f, 1.0f));
                }

                const glm::vec3 mountainWarp(
                    perlinNoise(terrainP * 0.62f + glm::vec3(2.1f, 17.4f, 8.6f)),
                    perlinNoise(terrainP * 0.62f + glm::vec3(11.8f, 5.3f, 24.1f)),
                    perlinNoise(terrainP * 0.62f + glm::vec3(19.6f, 14.2f, 3.5f))
                );
                const glm::vec3 mountainFieldP = terrainP * 0.92f + mountainWarp * 0.34f;
                const float mountainNoise = perlinNoise(mountainFieldP + glm::vec3(31.2f, 4.8f, 15.6f)) * 0.5f + 0.5f;
                const float ridgeNoise = 1.0f - std::abs(perlinNoise(mountainFieldP * 1.85f + glm::vec3(6.3f, 28.4f, 12.7f)));
                const float maskBreakup = perlinNoise(mountainFieldP * 1.35f + glm::vec3(22.2f, 9.1f, 4.7f)) * 0.5f + 0.5f;
                const float rawMountainMask =
                    peakMask * 1.05f
                  + mountainProvince * 0.10f
                  + (mountainNoise - 0.5f) * 0.08f
                  + (maskBreakup - 0.5f) * 0.06f;
                const float mountainMaskField = glm::smoothstep(0.28f, 0.82f, rawMountainMask);
                const float fieldRelief = settings.mountainMaskStrength * amplitudeScale * landMask
                    * mountainMaskField
                    * (glm::smoothstep(0.30f, 0.92f, mountainNoise) * 0.034f
                       + std::pow(glm::clamp(ridgeNoise, 0.0f, 1.0f), 2.1f) * 0.026f);
                const float coreRelief = settings.mountainMaskStrength * amplitudeScale * landMask
                    * std::pow(glm::clamp(peakCore, 0.0f, 1.0f), 1.35f)
                    * (0.010f + ridgeNoise * 0.012f);
                const float gullyNoise = 1.0f - std::abs(perlinNoise(mountainFieldP * 3.25f + glm::vec3(5.7f, 18.9f, 30.4f)));
                const float fieldCarve = settings.mountainMaskStrength * amplitudeScale * landMask
                    * mountainMaskField
                    * (1.0f - peakCore * 0.55f)
                    * std::pow(glm::clamp(gullyNoise, 0.0f, 1.0f), 4.0f)
                    * 0.004f;

                height += landMask * (hillUplift + mountainUplift + volcanicUplift + terrainDetail) + fieldRelief + coreRelief;
                height -= fieldCarve;
                height -= landMask * valleyCarve;
                faceData.height[index] = height;
                const float mountainMaterialDomain = glm::clamp(
                    glm::max(glm::max(glm::max(ridgeShape, massifShape), peakMaterial), coneShape * 0.82f) + mountainMaskField * 0.14f,
                    0.0f,
                    1.0f
                );
                faceData.domainWeight[index].x = glm::max(faceData.domainWeight[index].x, mountainMaterialDomain);
                faceData.domainWeight[index].y = glm::max(faceData.domainWeight[index].y, skeletonValley);
                faceData.domainWeight[index].w = glm::max(faceData.domainWeight[index].w, 1.0f - glm::max(mountainDomain, skeletonValley));
            }

            if (advanceProgress) {
                advanceProgress("Compositing geomorphology domains");
            }
        }
    }
}

void PlanetProceduralData::removeSmallTerrainSpikes(const PlanetRenderSettings& settings,
                                                    int iterations,
                                                    float threshold,
                                                    float blend)
{
    PROFILE_SCOPE("Remove Small Terrain Spikes");
    if (resolution_ <= 1 || iterations <= 0 || threshold <= 0.0f || blend <= 0.0f) {
        return;
    }

    const int n = resolution_;
    const std::size_t cellCount = static_cast<std::size_t>(n * n);
    const float seaLevel = settings.seaLevelOffset;
    const float clampedBlend = glm::clamp(blend, 0.0f, 1.0f);
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };

    std::array<std::vector<float>, 6> nextHeight;
    std::array<float, 8> neighbors{};
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        nextHeight[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
    }

    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            std::vector<float>& target = nextHeight[static_cast<std::size_t>(faceIndex)];

            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::size_t index = indexOf(x, y);
                    const float height = faceData.height[index];
                    const float landMask = glm::smoothstep(seaLevel - 0.03f, seaLevel + 0.14f, height);
                    if (landMask <= 0.001f) {
                        target[index] = height;
                        continue;
                    }

                    int neighborCount = 0;
                    float weightedSum = 0.0f;
                    float totalWeight = 0.0f;
                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox == 0 && oy == 0) {
                                continue;
                            }
                            const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                            const float neighborHeight = faces_[static_cast<std::size_t>(neighbor.face)].height[neighbor.index];
                            const float weight = (ox == 0 || oy == 0) ? 1.0f : 0.7071f;
                            neighbors[static_cast<std::size_t>(neighborCount++)] = neighborHeight;
                            weightedSum += neighborHeight * weight;
                            totalWeight += weight;
                        }
                    }

                    std::sort(neighbors.begin(), neighbors.begin() + neighborCount);
                    const float median = neighbors[static_cast<std::size_t>(neighborCount / 2)];
                    const float neighborAverage = weightedSum / std::max(totalWeight, 0.0001f);
                    const float highExcess = height - median;
                    const float lowExcess = median - height;
                    const float spikeMask = glm::smoothstep(threshold, threshold * 2.6f, std::max(highExcess, lowExcess));
                    if (spikeMask <= 0.001f) {
                        target[index] = height;
                        continue;
                    }

                    const float filteredHeight = glm::mix(median, neighborAverage, 0.30f);
                    target[index] = glm::mix(height, filteredHeight, spikeMask * clampedBlend * landMask);
                }
            }
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            faces_[static_cast<std::size_t>(faceIndex)].height.swap(nextHeight[static_cast<std::size_t>(faceIndex)]);
        }
    }
}

void PlanetProceduralData::smoothSmallTerrainBumps(const PlanetRenderSettings& settings,
                                                   int iterations,
                                                   float blend)
{
    PROFILE_SCOPE("Smooth Small Terrain Bumps");
    if (resolution_ <= 1 || iterations <= 0 || blend <= 0.0f) {
        return;
    }

    const int n = resolution_;
    const std::size_t cellCount = static_cast<std::size_t>(n * n);
    const float seaLevel = settings.seaLevelOffset;
    const float clampedBlend = glm::clamp(blend, 0.0f, 1.0f);
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };

    std::array<std::vector<float>, 6> nextHeight;
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        nextHeight[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
    }

    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            std::vector<float>& target = nextHeight[static_cast<std::size_t>(faceIndex)];

            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::size_t index = indexOf(x, y);
                    const float height = faceData.height[index];
                    const float landMask = glm::smoothstep(seaLevel - 0.02f, seaLevel + 0.16f, height);
                    const float coastFade = glm::smoothstep(seaLevel + 0.035f, seaLevel + 0.16f, height);
                    if (landMask <= 0.001f || coastFade <= 0.001f) {
                        target[index] = height;
                        continue;
                    }

                    float weightedSum = height * 2.0f;
                    float totalWeight = 2.0f;
                    for (int oy = -2; oy <= 2; ++oy) {
                        for (int ox = -2; ox <= 2; ++ox) {
                            if (ox == 0 && oy == 0) {
                                continue;
                            }
                            const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                            const FaceData& neighborFace = faces_[static_cast<std::size_t>(neighbor.face)];
                            const float neighborHeight = neighborFace.height[neighbor.index];
                            const float neighborLand = glm::smoothstep(seaLevel - 0.02f, seaLevel + 0.16f, neighborHeight);
                            const float distSq = static_cast<float>(ox * ox + oy * oy);
                            const float weight = neighborLand / (1.0f + distSq * 0.55f);
                            weightedSum += neighborHeight * weight;
                            totalWeight += weight;
                        }
                    }

                    const float localAverage = weightedSum / std::max(totalWeight, 0.0001f);
                    const float localDelta = std::abs(height - localAverage);
                    const float bumpMask = glm::smoothstep(0.0025f, 0.018f, localDelta);
                    const float riverProtect = glm::clamp(faceData.flowMask[index] * 0.65f + faceData.erosionMask[index] * 0.35f, 0.0f, 1.0f);
                    const float mountainCoreProtect = glm::smoothstep(0.72f, 0.98f, faceData.domainWeight[index].x) * 0.28f;
                    const float preserve = glm::clamp(1.0f - riverProtect * 0.75f - mountainCoreProtect, 0.20f, 1.0f);
                    const float smoothWeight = clampedBlend * landMask * coastFade * bumpMask * preserve;
                    target[index] = glm::mix(height, localAverage, smoothWeight);
                }
            }
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            faces_[static_cast<std::size_t>(faceIndex)].height.swap(nextHeight[static_cast<std::size_t>(faceIndex)]);
        }
    }
}

void PlanetProceduralData::relaxExtremeTerrainSlopes(const PlanetRenderSettings& settings,
                                                     int iterations,
                                                     float maxStep,
                                                     float blend)
{
    PROFILE_SCOPE("Relax Extreme Terrain Slopes");
    if (resolution_ <= 1 || iterations <= 0 || maxStep <= 0.0f || blend <= 0.0f) {
        return;
    }

    const int n = resolution_;
    const std::size_t cellCount = static_cast<std::size_t>(n * n);
    const float seaLevel = settings.seaLevelOffset;
    const float clampedBlend = glm::clamp(blend, 0.0f, 1.0f);
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };

    std::array<std::vector<float>, 6> nextHeight;
    std::array<std::vector<float>, 6> correction;
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        nextHeight[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
        correction[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
    }

    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            std::vector<float>& target = nextHeight[static_cast<std::size_t>(faceIndex)];

            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::size_t index = indexOf(x, y);
                    const float height = faceData.height[index];
                    float minNeighbor = std::numeric_limits<float>::max();
                    float maxNeighbor = std::numeric_limits<float>::lowest();
                    float weightedSum = 0.0f;
                    float totalWeight = 0.0f;

                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox == 0 && oy == 0) {
                                continue;
                            }
                            const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                            const FaceData& neighborFace = faces_[static_cast<std::size_t>(neighbor.face)];
                            const float neighborHeight = neighborFace.height[neighbor.index];
                            const float weight = (ox == 0 || oy == 0) ? 1.0f : 0.7071f;
                            minNeighbor = std::min(minNeighbor, neighborHeight);
                            maxNeighbor = std::max(maxNeighbor, neighborHeight);
                            weightedSum += neighborHeight * weight;
                            totalWeight += weight;
                        }
                    }

                    const float neighborAverage = weightedSum / std::max(totalWeight, 0.0001f);
                    const float highLimit = maxNeighbor + maxStep;
                    const float lowLimit = minNeighbor - maxStep;
                    float relaxedHeight = height;
                    if (height > highLimit) {
                        relaxedHeight = glm::mix(height, glm::min(neighborAverage + maxStep, height), clampedBlend);
                    } else if (height < lowLimit) {
                        relaxedHeight = glm::mix(height, glm::max(neighborAverage - maxStep, height), clampedBlend);
                    }

                    const float landMask = glm::smoothstep(seaLevel - 0.05f, seaLevel + 0.08f, height);
                    target[index] = glm::mix(height, relaxedHeight, landMask);
                }
            }
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            faces_[static_cast<std::size_t>(faceIndex)].height = nextHeight[static_cast<std::size_t>(faceIndex)];
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            std::fill(correction[static_cast<std::size_t>(faceIndex)].begin(),
                      correction[static_cast<std::size_t>(faceIndex)].end(),
                      0.0f);
        }

        const int axisOffsets[2][2] = {
            { 1, 0 },
            { 0, 1 }
        };

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            const FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            std::vector<float>& faceCorrection = correction[static_cast<std::size_t>(faceIndex)];
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::size_t index = indexOf(x, y);
                    const float height = faceData.height[index];
                    const float landMask = glm::smoothstep(seaLevel - 0.04f, seaLevel + 0.16f, height);
                    if (landMask <= 0.001f) {
                        continue;
                    }

                    for (const auto& offset : axisOffsets) {
                        const CellRef neighbor = neighborCell(faceIndex, x + offset[0], y + offset[1], n);
                        const float neighborHeight = faces_[static_cast<std::size_t>(neighbor.face)].height[neighbor.index];
                        const float neighborLandMask = glm::smoothstep(seaLevel - 0.04f, seaLevel + 0.16f, neighborHeight);
                        const float pairMask = std::min(landMask, neighborLandMask);
                        const float diff = height - neighborHeight;
                        const float excess = std::abs(diff) - maxStep;
                        if (pairMask <= 0.001f || excess <= 0.0f) {
                            continue;
                        }

                        const float transfer = excess * 0.5f * clampedBlend * pairMask;
                        if (diff > 0.0f) {
                            faceCorrection[index] -= transfer;
                            correction[static_cast<std::size_t>(neighbor.face)][neighbor.index] += transfer;
                        } else {
                            faceCorrection[index] += transfer;
                            correction[static_cast<std::size_t>(neighbor.face)][neighbor.index] -= transfer;
                        }
                    }
                }
            }
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            const std::vector<float>& faceCorrection = correction[static_cast<std::size_t>(faceIndex)];
            for (std::size_t i = 0; i < cellCount; ++i) {
                faceData.height[i] += faceCorrection[i];
            }
        }
    }
}

void PlanetProceduralData::computeWaterClimateFields(const PlanetRenderSettings& settings,
                                                     const std::function<void(const char*)>& advanceProgress)
{
    // 根据当前高度重新计算水深、海岸 mask、温度和湿度。
    // 侵蚀/biome 塑形之后会再次调用，所以这些字段始终跟最终地形同步。
    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FaceData& faceData = faces_[faceIndex];

        for (int y = 0; y < resolution_; ++y) {
            for (int x = 0; x < resolution_; ++x) {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution_),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution_)
                );
                const glm::vec3 sphereDir = cubeSphereDirection(kFaces[faceIndex], uv);
                const std::size_t index = static_cast<std::size_t>(y * resolution_ + x);
                const PlanetSample sample = samplePlanetBase(settings, sphereDir, faceData.height[index]);

                faceData.height[index] = sample.height;
                faceData.waterDepth[index] = sample.waterDepth;
                faceData.shoreMask[index] = sample.shoreMask;
                faceData.temperature[index] = sample.temperature;
                faceData.moisture[index] = sample.moisture;
            }
            if (advanceProgress) {
                advanceProgress("Computing ocean, coast, temperature, and moisture");
            }
        }
    }
}

void PlanetProceduralData::refineTerrainFromBiomeWeights(const PlanetRenderSettings& settings,
                                                         const std::function<void(const char*)>& advanceProgress)
{
    // 初始 biome 反过来塑造地形：沙漠更平、湿地更低、岩雪区域更粗糙。
    const float seaLevel = settings.seaLevelOffset;
    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FaceData& faceData = faces_[faceIndex];

        for (int y = 0; y < resolution_; ++y) {
            for (int x = 0; x < resolution_; ++x) {
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution_),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution_)
                );
                const glm::vec3 sphereDir = cubeSphereDirection(kFaces[faceIndex], uv);
                const std::size_t index = static_cast<std::size_t>(y * resolution_ + x);

                const glm::vec4 biomeA = faceData.biomeWeightA[index];
                const glm::vec4 biomeB = faceData.biomeWeightB[index];
                const float beach = glm::clamp(biomeA.r, 0.0f, 1.0f);
                const float grass = glm::clamp(biomeA.g, 0.0f, 1.0f);
                const float forest = glm::clamp(biomeA.b, 0.0f, 1.0f);
                const float desert = glm::clamp(biomeA.a, 0.0f, 1.0f);
                const float rock = glm::clamp(biomeB.r, 0.0f, 1.0f);
                const float snow = glm::clamp(biomeB.g, 0.0f, 1.0f);
                const float wetland = glm::clamp(biomeB.b, 0.0f, 1.0f);

                const float dune = perlinNoise(sphereDir * 6.5f + glm::vec3(23.1f, 7.4f, 11.8f));
                const float forestRelief = perlinNoise(sphereDir * 4.8f + glm::vec3(4.2f, 19.7f, 8.5f));
                const float alpineRelief = perlinNoise(sphereDir * 4.2f + glm::vec3(17.2f, 3.6f, 21.4f));

                float height = faceData.height[index];
                const float lowPlainTarget = seaLevel + 0.055f + dune * 0.004f;
                const float desertPlain = desert * (1.0f - glm::smoothstep(0.18f, 0.46f, height - seaLevel));
                height = glm::mix(height, lowPlainTarget, desertPlain * 0.42f);
                height += desert * dune * 0.0025f;
                height += grass * forestRelief * 0.0010f;
                height += forest * forestRelief * 0.0015f;
                height += (rock * 0.0025f + snow * 0.0018f) * std::max(alpineRelief, 0.0f);
                height = glm::mix(height, seaLevel + 0.018f, wetland * 0.36f);
                height = glm::mix(height, seaLevel + 0.010f, beach * 0.48f);

                faceData.height[index] = height;
            }
            if (advanceProgress) {
                advanceProgress("Refining plains, dunes, and alpine relief from initial biomes");
            }
        }
    }
}

void PlanetProceduralData::updateHydrologyMoisture(const PlanetRenderSettings& settings)
{
    const int n = resolution_;
    if (n <= 0) {
        return;
    }

    const float seaLevel = settings.seaLevelOffset;
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };

    std::array<std::vector<float>, 6> updatedMoisture;
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        updatedMoisture[static_cast<std::size_t>(faceIndex)] =
            faces_[static_cast<std::size_t>(faceIndex)].moisture;
    }

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        std::vector<float>& faceMoisture = updatedMoisture[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const std::size_t center = indexOf(x, y);
                const float height = faceData.height[center];
                const float waterDepth = faceData.waterDepth[center];
                const float shore = faceData.shoreMask[center];
                const float lowland = 1.0f - glm::smoothstep(0.06f, 0.34f, height - seaLevel);
                const float nearWater = glm::smoothstep(0.0f, 0.24f, waterDepth)
                                      + shore * 0.55f
                                      + lowland * shore * 0.35f;
                const float drainage = faceData.flowMask[center] * 0.38f
                                      + faceData.channelMask[center] * 0.30f
                                      + faceData.depositionMask[center] * 0.24f;

                // 查 5x5 邻域，水体/河道会向周围扩散湿度；neighborCell 处理跨面邻居。
                float neighborWater = 0.0f;
                float neighborFlow = 0.0f;
                for (int oy = -2; oy <= 2; ++oy) {
                    for (int ox = -2; ox <= 2; ++ox) {
                        if (ox == 0 && oy == 0) {
                            continue;
                        }
                        const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                        const FaceData& neighborFace = faces_[static_cast<std::size_t>(neighbor.face)];
                        const float distance = std::sqrt(static_cast<float>(ox * ox + oy * oy));
                        const float weight = 1.0f / std::max(distance, 1.0f);
                        neighborWater += glm::smoothstep(0.001f, 0.10f, neighborFace.waterDepth[neighbor.index]) * weight;
                        neighborFlow += (neighborFace.flowMask[neighbor.index] + neighborFace.channelMask[neighbor.index] * 0.65f) * weight;
                    }
                }
                neighborWater = glm::clamp(neighborWater / 6.8f, 0.0f, 1.0f);
                neighborFlow = glm::clamp(neighborFlow / 6.8f, 0.0f, 1.0f);

                const float hydrologyMoisture = glm::clamp(
                    nearWater * 0.55f
                  + drainage * 0.55f
                  + neighborWater * 0.30f
                  + neighborFlow * 0.28f,
                    0.0f,
                    1.0f
                );
                const float aridInterior = (1.0f - shore)
                                         * (1.0f - neighborWater)
                                         * (1.0f - glm::smoothstep(0.0f, 0.22f, drainage));
                float moisture = faceData.moisture[center];
                moisture = glm::mix(moisture, 1.0f, hydrologyMoisture * 0.58f);
                moisture *= 1.0f - aridInterior * glm::smoothstep(0.38f, 0.86f, height - seaLevel) * 0.18f;
                faceMoisture[center] = glm::clamp(moisture, 0.0f, 1.0f);
            }
        }
    }

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        faces_[static_cast<std::size_t>(faceIndex)].moisture =
            std::move(updatedMoisture[static_cast<std::size_t>(faceIndex)]);
    }
}

void PlanetProceduralData::computeBiomeWeights(const PlanetRenderSettings& settings,
                                               const std::function<void(const char*)>& advanceProgress)
{
    const int n = resolution_;
    const std::size_t cellCount = static_cast<std::size_t>(n * n);
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };
    const auto sampleCachedHeight = [&](const glm::vec3& dir) {
        const CellRef ref = cellFromDirection(glm::normalize(dir), n);
        return faces_[static_cast<std::size_t>(ref.face)].height[ref.index];
    };
    // 在球面切线方向做中心差分，得到坡度估计。
    const auto computeSphericalSlope = [&](const glm::vec3& sphereDir) {
        const glm::vec3 nDir = glm::normalize(sphereDir);
        const glm::vec3 up = std::abs(nDir.y) < 0.9f
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 tangent = glm::normalize(glm::cross(up, nDir));
        const glm::vec3 bitangent = glm::normalize(glm::cross(nDir, tangent));
        const float eps = std::max(1.5f / static_cast<float>(n), 0.0025f);

        const float hL = sampleCachedHeight(glm::normalize(nDir - tangent * eps));
        const float hR = sampleCachedHeight(glm::normalize(nDir + tangent * eps));
        const float hD = sampleCachedHeight(glm::normalize(nDir - bitangent * eps));
        const float hU = sampleCachedHeight(glm::normalize(nDir + bitangent * eps));
        const float dhTangent = (hR - hL) / (2.0f * eps);
        const float dhBitangent = (hU - hD) / (2.0f * eps);
        return glm::clamp(std::sqrt(dhTangent * dhTangent + dhBitangent * dhBitangent) * 0.08f, 0.0f, 1.0f);
    };
    // 统计周围水体，额外估计海湾/浅湾遮蔽度，供 beach/wetland 权重使用。
    const auto computeCoastalWater = [&](int faceIndex, int x, int y) {
        float immediateWater = 0.0f;
        float surroundingWater = 0.0f;
        float totalWeight = 0.0f;

        for (int oy = -2; oy <= 2; ++oy) {
            for (int ox = -2; ox <= 2; ++ox) {
                if (ox == 0 && oy == 0) {
                    continue;
                }

                const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                const FaceData& neighborFace = faces_[static_cast<std::size_t>(neighbor.face)];
                const float neighborWaterDepth = neighborFace.waterDepth[neighbor.index];
                const float neighborBelowSea = settings.seaLevelOffset - neighborFace.height[neighbor.index];
                const float neighborWater = glm::smoothstep(
                    0.0005f,
                    0.028f,
                    std::max(neighborWaterDepth, neighborBelowSea)
                );
                const float dist2 = static_cast<float>(ox * ox + oy * oy);
                const float weight = 1.0f / (1.0f + dist2 * 0.55f);

                surroundingWater += neighborWater * weight;
                totalWeight += weight;
                if (std::abs(ox) <= 1 && std::abs(oy) <= 1) {
                    immediateWater = std::max(immediateWater, neighborWater);
                }
            }
        }

        const float waterAround = surroundingWater / std::max(totalWeight, 0.0001f);
        const float bayShelter = glm::smoothstep(0.18f, 0.52f, waterAround)
                               * (1.0f - glm::smoothstep(0.88f, 1.0f, waterAround));
        return glm::vec2(immediateWater, glm::clamp(bayShelter, 0.0f, 1.0f));
    };

    for (FaceData& faceData : faces_) {
        faceData.biomeWeightA.assign(cellCount, glm::vec4(0.0f));
        faceData.biomeWeightB.assign(cellCount, glm::vec4(0.0f));
    }

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const std::size_t i = indexOf(x, y);
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(n),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(n)
                );
                const glm::vec3 sphereDir = cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], uv);
                PlanetSample sample = samplePlanetBase(settings, sphereDir, faceData.height[i]);
                sample.slope = computeSphericalSlope(sphereDir);
                sample.channel = faceData.channelMask[i];
                sample.flow = faceData.flowMask[i];
                sample.wear = faceData.wearMask[i];
                sample.deposition = faceData.depositionMask[i];
                const glm::vec2 coastalWater = computeCoastalWater(faceIndex, x, y);

                const BiomeWeights biome = computeBiome(
                    sample.height,
                    sample.waterDepth,
                    sample.shoreMask,
                    coastalWater.x,
                    coastalWater.y,
                    sphereDir,
                    settings.seaLevelOffset,
                    sample.temperature,
                    sample.moisture,
                    sample.slope,
                    sample.channel,
                    sample.flow,
                    sample.wear,
                    sample.deposition
                );

                faceData.biomeWeightA[i] = glm::vec4(
                    biome.beach,
                    biome.grass,
                    biome.forest,
                    biome.desert
                );
                faceData.biomeWeightB[i] = glm::vec4(
                    biome.rock,
                    biome.snow,
                    biome.wetland,
                    biome.shallowWater
                );
            }
            if (advanceProgress) {
                advanceProgress("Computing grassland, forest, desert, rock, and snowline weights");
            }
        }
    }
}

void PlanetProceduralData::smoothBiomeWeights(int radius, float blend)
{
    const int n = resolution_;
    if (n <= 0 || radius <= 0 || blend <= 0.0f) {
        return;
    }

    const float clampedBlend = glm::clamp(blend, 0.0f, 1.0f);
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };

    std::array<std::vector<glm::vec4>, 6> smoothedA;
    std::array<std::vector<glm::vec4>, 6> smoothedB;
    const std::size_t cellCount = static_cast<std::size_t>(n * n);
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        smoothedA[static_cast<std::size_t>(faceIndex)].assign(cellCount, glm::vec4(0.0f));
        smoothedB[static_cast<std::size_t>(faceIndex)].assign(cellCount, glm::vec4(0.0f));
    }

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                glm::vec4 sumA(0.0f);
                glm::vec4 sumB(0.0f);
                float totalWeight = 0.0f;

                for (int oy = -radius; oy <= radius; ++oy) {
                    for (int ox = -radius; ox <= radius; ++ox) {
                        const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                        const float d2 = static_cast<float>(ox * ox + oy * oy);
                        const float weight = std::exp(-d2 * 0.70f);
                        const FaceData& neighborFace = faces_[static_cast<std::size_t>(neighbor.face)];
                        sumA += neighborFace.biomeWeightA[neighbor.index] * weight;
                        sumB += neighborFace.biomeWeightB[neighbor.index] * weight;
                        totalWeight += weight;
                    }
                }

                const std::size_t index = indexOf(x, y);
                // 高斯邻域平滑后重新归一化陆地权重，保持 shallowWater 与 land 权重总量稳定。
                glm::vec4 a = glm::mix(faceData.biomeWeightA[index], sumA / std::max(totalWeight, 0.0001f), clampedBlend);
                glm::vec4 b = glm::mix(faceData.biomeWeightB[index], sumB / std::max(totalWeight, 0.0001f), clampedBlend);

                a = glm::max(a, glm::vec4(0.0f));
                b = glm::max(b, glm::vec4(0.0f));
                b.a = glm::clamp(b.a, 0.0f, 1.0f);

                const float landTarget = glm::clamp(1.0f - b.a, 0.0f, 1.0f);
                const float landSum = a.r + a.g + a.b + a.a + b.r + b.g + b.b;
                if (landSum > 0.00001f) {
                    const float inv = landTarget / landSum;
                    a *= inv;
                    b.r *= inv;
                    b.g *= inv;
                    b.b *= inv;
                }

                smoothedA[static_cast<std::size_t>(faceIndex)][index] = a;
                smoothedB[static_cast<std::size_t>(faceIndex)][index] = b;
            }
        }
    }

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        faces_[static_cast<std::size_t>(faceIndex)].biomeWeightA =
            std::move(smoothedA[static_cast<std::size_t>(faceIndex)]);
        faces_[static_cast<std::size_t>(faceIndex)].biomeWeightB =
            std::move(smoothedB[static_cast<std::size_t>(faceIndex)]);
    }
}

void PlanetProceduralData::computeMeshPlanningFields(const PlanetRenderSettings& settings,
                                                     const std::function<void(const char*)>& advanceProgress)
{
    const int n = resolution_;
    if (n <= 0) {
        return;
    }

    const std::size_t cellCount = static_cast<std::size_t>(n * n);
    const float seaLevel = settings.seaLevelOffset;
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };

    for (FaceData& faceData : faces_) {
        faceData.regionId.assign(cellCount, 0.0f);
        faceData.featureMask.assign(cellCount, 0.0f);
        faceData.meshDensity.assign(cellCount, 0.0f);
        faceData.geometricError.assign(cellCount, 0.0f);
        if (faceData.domainWeight.size() != cellCount) {
            faceData.domainWeight.assign(cellCount, glm::vec4(0.0f));
        } else {
            for (glm::vec4& domain : faceData.domainWeight) {
                domain.y = 0.0f;
                domain.z = 0.0f;
                domain.w = 0.0f;
            }
        }
    }

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const std::size_t i = indexOf(x, y);
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(n),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(n)
                );
                const glm::vec3 sphereDir = cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], uv);

                float neighborMin = faceData.height[i];
                float neighborMax = faceData.height[i];
                float neighborSum = 0.0f;
                float totalWeight = 0.0f;
                glm::vec4 biomeASum(0.0f);
                glm::vec4 biomeBSum(0.0f);
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) {
                            continue;
                        }
                        const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                        const FaceData& neighborFace = faces_[static_cast<std::size_t>(neighbor.face)];
                        const float h = neighborFace.height[neighbor.index];
                        const float weight = (ox == 0 || oy == 0) ? 1.0f : 0.7071f;
                        neighborMin = std::min(neighborMin, h);
                        neighborMax = std::max(neighborMax, h);
                        neighborSum += h * weight;
                        totalWeight += weight;
                        biomeASum += neighborFace.biomeWeightA[neighbor.index] * weight;
                        biomeBSum += neighborFace.biomeWeightB[neighbor.index] * weight;
                    }
                }

                const float height = faceData.height[i];
                const float avgNeighborHeight = neighborSum / std::max(totalWeight, 0.0001f);
                const float slope = glm::clamp((neighborMax - neighborMin) * settings.terrainHeightScale * 0.22f, 0.0f, 1.0f);
                const float curvature = glm::clamp(std::abs(height - avgNeighborHeight) * settings.terrainHeightScale * 0.85f, 0.0f, 1.0f);
                const glm::vec4 biomeA = glm::max(faceData.biomeWeightA[i], glm::vec4(0.0f));
                const glm::vec4 biomeB = glm::max(faceData.biomeWeightB[i], glm::vec4(0.0f));
                const glm::vec4 avgBiomeA = biomeASum / std::max(totalWeight, 0.0001f);
                const glm::vec4 avgBiomeB = biomeBSum / std::max(totalWeight, 0.0001f);
                const float biomeBoundary = glm::clamp(
                    glm::length(biomeA - avgBiomeA) + glm::length(biomeB - avgBiomeB),
                    0.0f,
                    1.0f
                );

                const float waterMask = glm::smoothstep(0.001f, 0.075f, faceData.waterDepth[i]);
                const float coastMask = glm::max(faceData.shoreMask[i], biomeA.r);
                const float riverMask = glm::max(faceData.channelMask[i], faceData.flowMask[i]);
                const float erosionMask = glm::max(faceData.erosionMask[i], glm::max(faceData.wearMask[i], faceData.depositionMask[i]));
                const float heightfieldMountainMask = faceData.domainWeight.empty()
                    ? 0.0f
                    : glm::clamp(faceData.domainWeight[i].x, 0.0f, 1.0f);
                const float terrainSteepnessMask = glm::clamp(biomeB.r * 0.32f + biomeB.g * 0.22f + slope * 0.20f, 0.0f, 1.0f);
                const float mountainPlanningMask = glm::max(heightfieldMountainMask, terrainSteepnessMask);
                const float wetlandMask = glm::clamp(biomeB.b + coastMask * faceData.moisture[i] * 0.45f, 0.0f, 1.0f);
                const float aridMask = glm::clamp(biomeA.a + (1.0f - faceData.moisture[i]) * glm::smoothstep(0.08f, 0.35f, height - seaLevel) * 0.45f, 0.0f, 1.0f);

                // Domain weights: x=mountain/ridge, y=hydrology, z=coast/wetland, w=arid/plain.
                glm::vec4 domain(
                    heightfieldMountainMask,
                    glm::clamp(riverMask * 0.85f + erosionMask * 0.35f, 0.0f, 1.0f),
                    glm::clamp(coastMask * 0.90f + wetlandMask * 0.45f, 0.0f, 1.0f),
                    aridMask
                );
                const float domainMax = std::max(std::max(domain.x, domain.y), std::max(domain.z, domain.w));
                float regionId = 0.0f;
                if (domainMax == domain.y) {
                    regionId = 2.0f;
                } else if (domainMax == domain.z) {
                    regionId = 3.0f;
                } else if (domainMax == domain.w) {
                    regionId = 4.0f;
                } else {
                    regionId = 1.0f;
                }
                if (waterMask > 0.75f && coastMask < 0.12f) {
                    regionId = 5.0f;
                }

                const float macroRegionNoise = fbm(sphereDir * 2.35f + glm::vec3(19.2f, 7.1f, 31.4f), 4, 2.0f, 0.5f);
                const float macroBoundary = glm::smoothstep(0.42f, 0.80f, std::abs(macroRegionNoise - 0.50f) * 2.0f);
                const float featureMask = glm::clamp(
                    slope * 0.28f
                  + curvature * 0.30f
                  + coastMask * 0.42f
                  + riverMask * 0.62f
                  + erosionMask * 0.42f
                  + biomeBoundary * 0.32f
                  + macroBoundary * 0.10f,
                    0.0f,
                    1.0f
                );
                const float meshDensity = glm::clamp(
                    0.10f
                  + featureMask * 0.66f
                  + mountainPlanningMask * 0.16f
                  + coastMask * 0.18f
                  + riverMask * 0.24f
                  - waterMask * (1.0f - coastMask) * 0.18f,
                    0.0f,
                    1.0f
                );
                const float geometricError = glm::clamp(
                    (neighborMax - neighborMin) * settings.terrainHeightScale
                  + curvature * settings.terrainHeightScale * 0.35f
                  + riverMask * 0.70f
                  + coastMask * 0.45f,
                    0.0f,
                    64.0f
                );

                faceData.regionId[i] = regionId;
                faceData.domainWeight[i] = domain;
                faceData.featureMask[i] = featureMask;
                faceData.meshDensity[i] = meshDensity;
                faceData.geometricError[i] = geometricError;
            }
            if (advanceProgress) {
                advanceProgress("Planning feature-aware mesh density fields");
            }
        }
    }
}

void PlanetProceduralData::applyErosion(const PlanetRenderSettings& settings,
                                        const std::function<void(const char*)>& advanceProgress)
{
    const int iterations = std::clamp(settings.erosionIterations, 0, 256);
    const float erosionStrength = std::max(settings.erosionStrength, 0.0f);
    const float thermalStrength = std::max(settings.erosionThermalStrength, 0.0f);
    const int n = resolution_;
    const std::size_t cellCount = static_cast<std::size_t>(n * n);
    const float talus = std::max(settings.erosionTalus, 0.0001f);
    const float sedimentResponse = glm::clamp(settings.erosionSediment, 0.0f, 1.0f);
    const float capacityFactor = glm::mix(1.8f, 5.8f, sedimentResponse);
    const float depositionRate = glm::mix(0.045f, 0.22f, sedimentResponse);
    const float rainAmount = 0.010f + erosionStrength * 0.045f;
    const float evaporation = 0.045f;
    const float flowFraction = 0.58f;
    const float seaLevel = settings.seaLevelOffset;
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };
    const auto normalizeMask = [](std::vector<float>& mask, float exponent) {
        float maxValue = 0.0f;
        for (float value : mask) {
            maxValue = std::max(maxValue, value);
        }
        if (maxValue <= 0.000001f) {
            return;
        }
        for (float& value : mask) {
            value = std::pow(glm::clamp(value / maxValue, 0.0f, 1.0f), exponent);
        }
    };

    for (FaceData& faceData : faces_) {
        // 侵蚀 mask 重新生成，不沿用上一次 bake 的残留值。
        faceData.erosionMask.assign(faceData.height.size(), 0.0f);
        faceData.channelMask.assign(faceData.height.size(), 0.0f);
        faceData.flowMask.assign(faceData.height.size(), 0.0f);
        faceData.wearMask.assign(faceData.height.size(), 0.0f);
        faceData.depositionMask.assign(faceData.height.size(), 0.0f);
    }

    if (iterations <= 0 || (erosionStrength <= 0.0f && thermalStrength <= 0.0f)) {
        return;
    }

    std::array<std::vector<float>, 6> water;
    std::array<std::vector<float>, 6> sediment;
    std::array<std::vector<float>, 6> nextWater;
    std::array<std::vector<float>, 6> nextSediment;
    std::array<std::vector<float>, 6> delta;
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        water[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
        sediment[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
        nextWater[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
        nextSediment[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
        delta[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
    }

    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            std::fill(delta[static_cast<std::size_t>(faceIndex)].begin(), delta[static_cast<std::size_t>(faceIndex)].end(), 0.0f);
            std::fill(nextWater[static_cast<std::size_t>(faceIndex)].begin(), nextWater[static_cast<std::size_t>(faceIndex)].end(), 0.0f);
            std::fill(nextSediment[static_cast<std::size_t>(faceIndex)].begin(), nextSediment[static_cast<std::size_t>(faceIndex)].end(), 0.0f);
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::size_t center = indexOf(x, y);
                    const float h = faceData.height[center];
                    const float landMask = glm::smoothstep(seaLevel + 0.02f, seaLevel + 0.20f, h);
                    if (landMask <= 0.001f) {
                        continue;
                    }

                    std::vector<float>& faceWater = water[static_cast<std::size_t>(faceIndex)];
                    std::vector<float>& faceSediment = sediment[static_cast<std::size_t>(faceIndex)];
                    std::vector<float>& faceNextWater = nextWater[static_cast<std::size_t>(faceIndex)];
                    std::vector<float>& faceNextSediment = nextSediment[static_cast<std::size_t>(faceIndex)];
                    std::vector<float>& faceDelta = delta[static_cast<std::size_t>(faceIndex)];

                    float currentWater = faceWater[center] + rainAmount * landMask;
                    float currentSediment = faceSediment[center];
                    const float currentSurface = h + currentWater;
                    float totalDrop = 0.0f;
                    float maxDrop = 0.0f;
                    struct DownhillNeighbor {
                        int face;
                        std::size_t index;
                        float drop;
                    };
                    DownhillNeighbor downhill[8];
                    int downhillCount = 0;

                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox == 0 && oy == 0) {
                                continue;
                            }
                            const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                            const float neighborSurface =
                                faces_[static_cast<std::size_t>(neighbor.face)].height[neighbor.index]
                              + water[static_cast<std::size_t>(neighbor.face)][neighbor.index];
                            const float drop = currentSurface - neighborSurface;
                            if (drop > 0.0f) {
                                downhill[downhillCount++] = { neighbor.face, neighbor.index, drop };
                                totalDrop += drop;
                                maxDrop = std::max(maxDrop, drop);
                            }
                        }
                    }

                    if (downhillCount <= 0 || totalDrop <= 0.000001f) {
                        // 没有下坡方向时水停留并少量沉积。
                        const float standingDeposit = currentSediment * depositionRate * 0.18f;
                        faceDelta[center] += standingDeposit;
                        currentSediment -= standingDeposit;
                        faceData.depositionMask[center] += standingDeposit;
                        faceNextWater[center] += currentWater * (1.0f - evaporation);
                        faceNextSediment[center] += currentSediment;
                        continue;
                    }

                    for (int a = 0; a < downhillCount - 1; ++a) {
                        for (int b = a + 1; b < downhillCount; ++b) {
                            if (downhill[b].drop > downhill[a].drop) {
                                std::swap(downhill[a], downhill[b]);
                            }
                        }
                    }
                    downhillCount = std::min(downhillCount, 2);
                    totalDrop = 0.0f;
                    for (int i = 0; i < downhillCount; ++i) {
                        totalDrop += downhill[i].drop;
                    }

                    const float flowSpeed = glm::clamp(maxDrop * 6.0f, 0.0f, 1.0f);
                    const float capacity = std::max(flowSpeed * currentWater * capacityFactor, 0.00003f);
                    // sediment capacity 决定当前格是侵蚀还是沉积。
                    if (currentSediment < capacity && erosionStrength > 0.0f) {
                        const float erodeAmount = std::min(
                            (capacity - currentSediment) * erosionStrength * landMask,
                            maxDrop * 0.22f
                        );
                        faceDelta[center] -= erodeAmount;
                        currentSediment += erodeAmount;
                        faceData.wearMask[center] += erodeAmount;
                    } else if (currentSediment > capacity) {
                        const float depositAmount = (currentSediment - capacity) * depositionRate;
                        faceDelta[center] += depositAmount;
                        currentSediment -= depositAmount;
                        faceData.depositionMask[center] += depositAmount;
                    }

                    const float movedWater = currentWater * flowFraction;
                    const float keptWater = currentWater - movedWater;
                    const float invWater = currentWater > 0.000001f ? 1.0f / currentWater : 0.0f;
                    faceNextWater[center] += keptWater * (1.0f - evaporation);
                    faceNextSediment[center] += currentSediment * keptWater * invWater;
                    faceData.flowMask[center] += movedWater * flowSpeed;

                    for (int i = 0; i < downhillCount; ++i) {
                        // 只向最陡的两个方向分流水和沉积物，形成更清晰的流路。
                        const float share = downhill[i].drop / totalDrop;
                        const float waterShare = movedWater * share;
                        const std::size_t neighborFaceIndex = static_cast<std::size_t>(downhill[i].face);
                        nextWater[neighborFaceIndex][downhill[i].index] += waterShare * (1.0f - evaporation);
                        nextSediment[neighborFaceIndex][downhill[i].index] += currentSediment * waterShare * invWater;
                        faces_[neighborFaceIndex].flowMask[downhill[i].index] += waterShare * flowSpeed * 0.35f;
                    }
                }
            }
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            std::vector<float>& faceDelta = delta[static_cast<std::size_t>(faceIndex)];
            for (std::size_t i = 0; i < cellCount; ++i) {
                faceData.height[i] += faceDelta[i];
                faceData.erosionMask[i] += std::abs(faceDelta[i]);
            }
        }
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            water[static_cast<std::size_t>(faceIndex)].swap(nextWater[static_cast<std::size_t>(faceIndex)]);
            sediment[static_cast<std::size_t>(faceIndex)].swap(nextSediment[static_cast<std::size_t>(faceIndex)]);
        }
        if (advanceProgress) {
            advanceProgress("Running hydraulic erosion");
        }
    }

    const int thermalIterations = thermalStrength > 0.0f ? std::clamp(iterations / 3, 1, 80) : 0;
    for (int iteration = 0; iteration < thermalIterations; ++iteration) {
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            std::fill(delta[static_cast<std::size_t>(faceIndex)].begin(), delta[static_cast<std::size_t>(faceIndex)].end(), 0.0f);
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            std::vector<float>& faceDelta = delta[static_cast<std::size_t>(faceIndex)];
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::size_t center = indexOf(x, y);
                    const float h = faceData.height[center];
                    const float landMask = glm::smoothstep(seaLevel + 0.02f, seaLevel + 0.20f, h);
                    if (landMask <= 0.001f) {
                        continue;
                    }

                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if ((ox == 0 && oy == 0) || (ox != 0 && oy != 0)) {
                                continue;
                            }
                            const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                            FaceData& neighborFace = faces_[static_cast<std::size_t>(neighbor.face)];
                            const float slopeToNeighbor = h - neighborFace.height[neighbor.index];
                            if (slopeToNeighbor > talus * 1.45f) {
                                // 热力侵蚀：超过安息角的坡面向低邻居滑落。
                                const float slide = std::min(
                                    (slopeToNeighbor - talus * 1.45f) * thermalStrength * landMask,
                                    slopeToNeighbor * 0.18f
                                );
                                faceDelta[center] -= slide;
                                delta[static_cast<std::size_t>(neighbor.face)][neighbor.index] += slide;
                                faceData.wearMask[center] += slide * 0.45f;
                                neighborFace.depositionMask[neighbor.index] += slide * 0.35f;
                            }
                        }
                    }
                }
            }
        }

        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            std::vector<float>& faceDelta = delta[static_cast<std::size_t>(faceIndex)];
            for (std::size_t i = 0; i < cellCount; ++i) {
                faceData.height[i] += faceDelta[i];
                faceData.erosionMask[i] += std::abs(faceDelta[i]);
            }
        }
        if (advanceProgress) {
            advanceProgress("Running thermal erosion");
        }
    }

    for (FaceData& faceData : faces_) {
        normalizeMask(faceData.erosionMask, 1.6f);
        normalizeMask(faceData.flowMask, 1.35f);
        normalizeMask(faceData.wearMask, 1.45f);
        normalizeMask(faceData.depositionMask, 1.35f);
    }

    std::array<std::vector<float>, 6> channelRaw;
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        channelRaw[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
    }
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const std::size_t center = indexOf(x, y);
                const float h = faceData.height[center];
                float neighborSum = 0.0f;
                float maxDiff = 0.0f;
                int neighborCount = 0;

                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) {
                            continue;
                        }
                        const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                        const float neighborHeight = faces_[static_cast<std::size_t>(neighbor.face)].height[neighbor.index];
                        neighborSum += neighborHeight;
                        maxDiff = std::max(maxDiff, std::abs(neighborHeight - h));
                        ++neighborCount;
                    }
                }

                const float avgNeighborHeight = neighborSum / std::max(neighborCount, 1);
                const float concavity = std::max(avgNeighborHeight - h, 0.0f);
                const float slopeGate = glm::smoothstep(0.006f, 0.055f, maxDiff);
                const float concavityGate = glm::smoothstep(0.0015f, 0.020f, concavity);
                const float flow = faceData.flowMask[center];
                const float wear = faceData.wearMask[center];
                // 河道 mask 偏向“凹陷 + 有流量 + 有坡差”的位置。
                float channel = flow * concavityGate * slopeGate;
                channel += wear * concavityGate * 0.18f;
                channelRaw[static_cast<std::size_t>(faceIndex)][center] = std::pow(glm::clamp(channel, 0.0f, 1.0f), 1.8f);
            }
        }
        if (advanceProgress) {
            advanceProgress("Detecting channel flow");
        }
    }
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        faces_[static_cast<std::size_t>(faceIndex)].channelMask = std::move(channelRaw[static_cast<std::size_t>(faceIndex)]);
        normalizeMask(faces_[static_cast<std::size_t>(faceIndex)].channelMask, 1.2f);
        for (float& value : faces_[static_cast<std::size_t>(faceIndex)].channelMask) {
            value = glm::smoothstep(0.10f, 0.55f, value);
        }
    }
}

void PlanetProceduralData::extractPrimaryRiver(const PlanetRenderSettings& settings,
                                               const std::function<void(const char*)>& advanceProgress)
{
    const int n = resolution_;
    if (n <= 4) {
        return;
    }

    const std::size_t cellCount = static_cast<std::size_t>(n * n);
    const float seaLevel = settings.seaLevelOffset;
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };
    const auto cellDirection = [this, n](int faceIndex, std::size_t index) {
        const int x = static_cast<int>(index % static_cast<std::size_t>(n));
        const int y = static_cast<int>(index / static_cast<std::size_t>(n));
        const glm::vec2 uv(
            (static_cast<float>(x) + 0.5f) / static_cast<float>(n),
            (static_cast<float>(y) + 0.5f) / static_cast<float>(n)
        );
        return cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], uv);
    };

    std::array<std::vector<float>, 6> baseFlow;
    std::array<std::vector<float>, 6> baseChannel;
    std::array<std::vector<float>, 6> baseWear;
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        baseFlow[static_cast<std::size_t>(faceIndex)] = faceData.flowMask;
        baseChannel[static_cast<std::size_t>(faceIndex)] = faceData.channelMask;
        baseWear[static_cast<std::size_t>(faceIndex)] = faceData.wearMask;
    }

    struct Candidate {
        int face = 0;
        std::size_t index = 0;
        float score = 0.0f;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(64);

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (std::size_t index = 0; index < cellCount; ++index) {
            const float height = faceData.height[index];
            const float relativeHeight = height - seaLevel;
            if (relativeHeight <= 0.16f || faceData.waterDepth[index] > 0.0f) {
                continue;
            }

            const glm::vec3 dir = cellDirection(faceIndex, index);
            const float basinNoise = fbm(dir * 2.25f + glm::vec3(19.4f, 7.1f, 31.6f), 4, 2.0f, 0.52f) * 0.5f + 0.5f;
            const float shorePenalty = glm::smoothstep(0.02f, 0.40f, faceData.shoreMask[index]);
            const float score = relativeHeight * 1.90f
                              + baseFlow[static_cast<std::size_t>(faceIndex)][index] * 0.75f
                              + baseWear[static_cast<std::size_t>(faceIndex)][index] * 0.26f
                              + basinNoise * 0.20f
                              - shorePenalty * 0.95f;
            candidates.push_back({ faceIndex, index, score });
        }
    }

    if (candidates.empty()) {
        return;
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });
    const std::size_t candidateCount = std::min<std::size_t>(candidates.size(), 32);

    struct RiverNode {
        int face = 0;
        std::size_t index = 0;
    };
    struct RiverTrace {
        std::vector<RiverNode> nodes;
        bool reachedSea = false;
        float score = -std::numeric_limits<float>::max();
    };

    const auto traceRiver = [&](const Candidate& source) {
        RiverTrace trace;
        std::array<std::vector<std::uint8_t>, 6> visited;
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            visited[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0);
        }

        int currentFace = source.face;
        std::size_t currentIndex = source.index;
        const float sourceHeight = faces_[static_cast<std::size_t>(currentFace)].height[currentIndex];
        const int maxSteps = std::max(n * 10, 128);

        for (int step = 0; step < maxSteps; ++step) {
            trace.nodes.push_back({ currentFace, currentIndex });
            visited[static_cast<std::size_t>(currentFace)][currentIndex] = 1;

            const FaceData& currentFaceData = faces_[static_cast<std::size_t>(currentFace)];
            const float currentHeight = currentFaceData.height[currentIndex];
            if (trace.nodes.size() > static_cast<std::size_t>(std::max(n / 3, 18))
                && currentHeight <= seaLevel + 0.010f) {
                trace.reachedSea = true;
                break;
            }

            const int cx = static_cast<int>(currentIndex % static_cast<std::size_t>(n));
            const int cy = static_cast<int>(currentIndex / static_cast<std::size_t>(n));
            float bestScore = -std::numeric_limits<float>::max();
            CellRef bestNeighbor{ currentFace, currentIndex };

            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (ox == 0 && oy == 0) {
                        continue;
                    }

                    const CellRef neighbor = neighborCell(currentFace, cx + ox, cy + oy, n);
                    const std::size_t neighborFace = static_cast<std::size_t>(neighbor.face);
                    const FaceData& neighborFaceData = faces_[neighborFace];
                    const float neighborHeight = neighborFaceData.height[neighbor.index];
                    const float drop = currentHeight - neighborHeight;
                    const glm::vec3 dir = cellDirection(neighbor.face, neighbor.index);
                    const float meander = fbm(dir * 6.8f + glm::vec3(4.7f, 23.1f, 9.6f), 3, 2.0f, 0.50f) * 0.5f + 0.5f;
                    const float oldDrainage = baseFlow[neighborFace][neighbor.index] * 0.78f
                                            + baseChannel[neighborFace][neighbor.index] * 1.05f;
                    const float seaBonus = neighborHeight <= seaLevel + 0.008f
                        ? (trace.nodes.size() > static_cast<std::size_t>(n / 2) ? 1.65f : -2.20f)
                        : 0.0f;
                    const float loopPenalty = visited[neighborFace][neighbor.index] != 0 ? 4.0f : 0.0f;
                    const float diagonalPenalty = (ox != 0 && oy != 0) ? 0.035f : 0.0f;
                    const float score = drop * 3.10f
                                      - neighborHeight * 0.72f
                                      + oldDrainage
                                      + baseWear[neighborFace][neighbor.index] * 0.18f
                                      + meander * 0.10f
                                      + seaBonus
                                      - loopPenalty
                                      - diagonalPenalty;
                    if (score > bestScore) {
                        bestScore = score;
                        bestNeighbor = neighbor;
                    }
                }
            }

            if (bestNeighbor.face == currentFace && bestNeighbor.index == currentIndex) {
                break;
            }
            if (visited[static_cast<std::size_t>(bestNeighbor.face)][bestNeighbor.index] != 0) {
                break;
            }

            currentFace = bestNeighbor.face;
            currentIndex = bestNeighbor.index;
        }

        const RiverNode& endNode = trace.nodes.back();
        const float endHeight = faces_[static_cast<std::size_t>(endNode.face)].height[endNode.index];
        const float length01 = static_cast<float>(trace.nodes.size()) / static_cast<float>(std::max(n, 1));
        const float altitudeDrop = std::max(sourceHeight - endHeight, 0.0f);
        trace.score = length01 * 1.35f
                    + altitudeDrop * 2.25f
                    + (trace.reachedSea ? 2.40f : 0.0f)
                    + source.score * 0.10f;
        return trace;
    };

    RiverTrace bestTrace;
    for (std::size_t i = 0; i < candidateCount; ++i) {
        RiverTrace trace = traceRiver(candidates[i]);
        if (trace.nodes.size() > bestTrace.nodes.size() && trace.score + 0.45f > bestTrace.score) {
            bestTrace = std::move(trace);
        } else if (trace.score > bestTrace.score) {
            bestTrace = std::move(trace);
        }
    }

    const int minimumTraceLength = std::max(n / 3, 18);
    if (bestTrace.nodes.size() < static_cast<std::size_t>(minimumTraceLength)) {
        return;
    }

    std::array<std::vector<float>, 6> primaryChannel;
    std::array<std::vector<float>, 6> primaryFlow;
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        primaryChannel[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
        primaryFlow[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0.0f);
        FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (std::size_t i = 0; i < cellCount; ++i) {
            const float oldWetTrace = std::pow(glm::clamp(baseFlow[static_cast<std::size_t>(faceIndex)][i], 0.0f, 1.0f), 1.85f);
            faceData.flowMask[i] = oldWetTrace * 0.10f;
            faceData.channelMask[i] = 0.0f;
        }
    }

    std::array<std::vector<std::uint8_t>, 6> trunkMask;
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        trunkMask[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0);
    }
    for (const RiverNode& node : bestTrace.nodes) {
        trunkMask[static_cast<std::size_t>(node.face)][node.index] = 1;
    }

    const int trunkCoreRadius = glm::clamp(n / 110, 1, 3);
    const int trunkBankRadius = trunkCoreRadius + 3;
    const int tributaryCoreRadius = 1;
    const int tributaryBankRadius = glm::clamp(n / 96, 2, 4);

    const auto paintRiver = [&](const RiverNode& node, float downstream, float scale, bool tributary) {
        const int centerX = static_cast<int>(node.index % static_cast<std::size_t>(n));
        const int centerY = static_cast<int>(node.index / static_cast<std::size_t>(n));
        const int coreRadius = tributary ? tributaryCoreRadius : trunkCoreRadius;
        const int bankRadius = tributary ? tributaryBankRadius : trunkBankRadius;
        const float coreSigma = std::max(static_cast<float>(coreRadius) * (0.52f + downstream * 0.30f), 0.72f);
        const float bankSigma = std::max(static_cast<float>(bankRadius) * (0.56f + downstream * 0.34f), 1.0f);
        const float channelStrength = glm::mix(0.48f, 1.0f, downstream) * scale;
        const float flowStrength = glm::mix(0.32f, 1.0f, downstream) * scale;

        for (int oy = -bankRadius; oy <= bankRadius; ++oy) {
            for (int ox = -bankRadius; ox <= bankRadius; ++ox) {
                const float d2 = static_cast<float>(ox * ox + oy * oy);
                if (d2 > static_cast<float>(bankRadius * bankRadius)) {
                    continue;
                }

                const CellRef target = neighborCell(node.face, centerX + ox, centerY + oy, n);
                const std::size_t targetFace = static_cast<std::size_t>(target.face);
                const float core = std::exp(-d2 / (2.0f * coreSigma * coreSigma));
                const float bank = std::exp(-d2 / (2.0f * bankSigma * bankSigma));
                primaryChannel[targetFace][target.index] = std::max(
                    primaryChannel[targetFace][target.index],
                    channelStrength * core
                );
                primaryFlow[targetFace][target.index] = std::max(
                    primaryFlow[targetFace][target.index],
                    flowStrength * bank
                );

                FaceData& targetFaceData = faces_[targetFace];
                const float carve = (tributary ? 0.0012f : 0.0018f) + downstream * (tributary ? 0.0020f : 0.0038f);
                const float carveAmount = carve * core * channelStrength;
                if (targetFaceData.height[target.index] > seaLevel + 0.006f) {
                    targetFaceData.height[target.index] = std::max(
                        targetFaceData.height[target.index] - carveAmount,
                        seaLevel + 0.006f
                    );
                }
                targetFaceData.wearMask[target.index] = std::max(
                    targetFaceData.wearMask[target.index],
                    core * channelStrength * 0.82f
                );
                targetFaceData.erosionMask[target.index] = std::max(
                    targetFaceData.erosionMask[target.index],
                    bank * flowStrength * 0.46f
                );
            }
        }
    };

    for (std::size_t i = 0; i < bestTrace.nodes.size(); ++i) {
        const float downstream = bestTrace.nodes.size() > 1
            ? static_cast<float>(i) / static_cast<float>(bestTrace.nodes.size() - 1)
            : 1.0f;
        paintRiver(bestTrace.nodes[i], downstream, 1.0f, false);
    }

    const auto traceTributary = [&](const Candidate& source, int maxSteps) {
        RiverTrace trace;
        std::array<std::vector<std::uint8_t>, 6> visited;
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            visited[static_cast<std::size_t>(faceIndex)].assign(cellCount, 0);
        }

        int currentFace = source.face;
        std::size_t currentIndex = source.index;
        for (int step = 0; step < maxSteps; ++step) {
            trace.nodes.push_back({ currentFace, currentIndex });
            visited[static_cast<std::size_t>(currentFace)][currentIndex] = 1;
            if (trunkMask[static_cast<std::size_t>(currentFace)][currentIndex] != 0
                && trace.nodes.size() > static_cast<std::size_t>(std::max(n / 8, 10))) {
                trace.reachedSea = true;
                break;
            }

            const FaceData& currentFaceData = faces_[static_cast<std::size_t>(currentFace)];
            const float currentHeight = currentFaceData.height[currentIndex];
            const int cx = static_cast<int>(currentIndex % static_cast<std::size_t>(n));
            const int cy = static_cast<int>(currentIndex / static_cast<std::size_t>(n));
            float bestScore = -std::numeric_limits<float>::max();
            CellRef bestNeighbor{ currentFace, currentIndex };

            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (ox == 0 && oy == 0) {
                        continue;
                    }

                    const CellRef neighbor = neighborCell(currentFace, cx + ox, cy + oy, n);
                    const std::size_t neighborFace = static_cast<std::size_t>(neighbor.face);
                    const FaceData& neighborFaceData = faces_[neighborFace];
                    const float neighborHeight = neighborFaceData.height[neighbor.index];
                    const float drop = currentHeight - neighborHeight;
                    const glm::vec3 dir = cellDirection(neighbor.face, neighbor.index);
                    const float branchNoise = fbm(dir * 11.5f + glm::vec3(31.7f, 5.2f, 18.9f), 3, 2.0f, 0.52f) * 0.5f + 0.5f;
                    const float oldDrainage = baseFlow[neighborFace][neighbor.index] * 0.95f
                                            + baseChannel[neighborFace][neighbor.index] * 0.55f;
                    const float trunkBonus = trunkMask[neighborFace][neighbor.index] != 0 ? 3.2f : 0.0f;
                    const float seaPenalty = neighborHeight <= seaLevel + 0.004f ? 1.8f : 0.0f;
                    const float loopPenalty = visited[neighborFace][neighbor.index] != 0 ? 4.5f : 0.0f;
                    const float score = drop * 2.20f
                                      - neighborHeight * 0.40f
                                      + oldDrainage
                                      + branchNoise * 0.20f
                                      + trunkBonus
                                      - seaPenalty
                                      - loopPenalty;
                    if (score > bestScore) {
                        bestScore = score;
                        bestNeighbor = neighbor;
                    }
                }
            }

            if (visited[static_cast<std::size_t>(bestNeighbor.face)][bestNeighbor.index] != 0) {
                break;
            }

            currentFace = bestNeighbor.face;
            currentIndex = bestNeighbor.index;
        }
        return trace;
    };

    int tributariesPainted = 0;
    const int maxTributaries = 18;
    const int tributaryMaxSteps = std::max(n * 2, 64);
    for (std::size_t i = 0; i < candidates.size() && tributariesPainted < maxTributaries; ++i) {
        const Candidate& candidate = candidates[i];
        if (trunkMask[static_cast<std::size_t>(candidate.face)][candidate.index] != 0) {
            continue;
        }
        const glm::vec3 sourceDir = cellDirection(candidate.face, candidate.index);
        bool tooCloseToTrunkSource = false;
        const std::size_t stride = std::max<std::size_t>(bestTrace.nodes.size() / 24, 1);
        for (std::size_t j = 0; j < bestTrace.nodes.size(); j += stride) {
            const RiverNode& trunkNode = bestTrace.nodes[j];
            const glm::vec3 trunkDir = cellDirection(trunkNode.face, trunkNode.index);
            if (glm::dot(sourceDir, trunkDir) > 0.995f) {
                tooCloseToTrunkSource = true;
                break;
            }
        }
        if (tooCloseToTrunkSource) {
            continue;
        }

        RiverTrace tributary = traceTributary(candidate, tributaryMaxSteps);
        if (tributary.nodes.size() < static_cast<std::size_t>(std::max(n / 7, 14))) {
            continue;
        }

        const float branchScale = glm::mix(0.44f, 0.82f, glm::clamp(candidate.score * 0.32f, 0.0f, 1.0f));
        for (std::size_t j = 0; j < tributary.nodes.size(); ++j) {
            const float downstream = tributary.nodes.size() > 1
                ? static_cast<float>(j) / static_cast<float>(tributary.nodes.size() - 1)
                : 1.0f;
            paintRiver(tributary.nodes[j], downstream, branchScale, true);
        }
        ++tributariesPainted;
    }

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (std::size_t i = 0; i < cellCount; ++i) {
            faceData.channelMask[i] = glm::smoothstep(0.26f, 0.86f, primaryChannel[static_cast<std::size_t>(faceIndex)][i]);
            faceData.flowMask[i] = std::max(
                faceData.flowMask[i],
                glm::smoothstep(0.08f, 0.70f, primaryFlow[static_cast<std::size_t>(faceIndex)][i])
            );
        }
    }

    if (advanceProgress) {
        advanceProgress("Extracting fractal tributary river network");
    }
}

glm::vec3 PlanetProceduralData::cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv)
{
    const glm::vec2 faceUv = uv * 2.0f - 1.0f;
    return glm::normalize(face.normal + faceUv.x * face.axisU + faceUv.y * face.axisV);
}

int PlanetProceduralData::faceIndexFromDirection(const glm::vec3& dir)
{
    const glm::vec3 a = glm::abs(dir);
    if (a.x >= a.y && a.x >= a.z) {
        return dir.x >= 0.0f ? 0 : 1;
    }
    if (a.y >= a.x && a.y >= a.z) {
        return dir.y >= 0.0f ? 2 : 3;
    }
    return dir.z >= 0.0f ? 4 : 5;
}

PlanetProceduralData::CellRef PlanetProceduralData::cellFromDirection(const glm::vec3& dir, int resolution)
{
    // 球面方向 -> 主轴最大的 cube face -> 该 face 的 UV/cell。
    const int n = std::max(resolution, 1);
    const glm::vec3 d = glm::normalize(dir);
    const int mappedFace = faceIndexFromDirection(d);
    const FaceBasis& basis = kFaces[static_cast<std::size_t>(mappedFace)];
    const float projection = std::max(std::abs(glm::dot(d, basis.normal)), 0.000001f);
    const glm::vec3 cubePoint = d / projection;
    const glm::vec2 faceUv(
        glm::dot(cubePoint - basis.normal, basis.axisU),
        glm::dot(cubePoint - basis.normal, basis.axisV)
    );
    const glm::vec2 uv = glm::clamp(faceUv * 0.5f + 0.5f, glm::vec2(0.0f), glm::vec2(0.999999f));
    const int x = glm::clamp(static_cast<int>(uv.x * static_cast<float>(n)), 0, n - 1);
    const int y = glm::clamp(static_cast<int>(uv.y * static_cast<float>(n)), 0, n - 1);
    return CellRef{ mappedFace, static_cast<std::size_t>(y * n + x) };
}

PlanetProceduralData::CellRef PlanetProceduralData::neighborCell(int faceIndex, int x, int y, int resolution)
{
    const int n = std::max(resolution, 1);
    if (x >= 0 && x < n && y >= 0 && y < n) {
        return CellRef{ faceIndex, static_cast<std::size_t>(y * n + x) };
    }

    // 越界时先把越界 UV 转回球面方向，再重新映射到正确 face。
    const glm::vec2 uv(
        (static_cast<float>(x) + 0.5f) / static_cast<float>(n),
        (static_cast<float>(y) + 0.5f) / static_cast<float>(n)
    );
    const glm::vec3 dir = cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], uv);
    return cellFromDirection(dir, n);
}

glm::vec3 PlanetProceduralData::hash3(const glm::vec3& p)
{
    const glm::vec3 q(
        glm::dot(p, glm::vec3(127.1f, 311.7f, 74.7f)),
        glm::dot(p, glm::vec3(269.5f, 183.3f, 246.1f)),
        glm::dot(p, glm::vec3(113.5f, 271.9f, 124.6f))
    );
    return -1.0f + 2.0f * glm::fract(glm::sin(q) * 43758.5453123f);
}

float PlanetProceduralData::gradientNoise(const glm::vec3& p)
{
    // 3D gradient noise：每个晶格角点用 hash3 生成伪随机梯度，再做平滑插值。
    const glm::vec3 i = glm::floor(p);
    const glm::vec3 f = glm::fract(p);
    const glm::vec3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);

    const float n000 = glm::dot(hash3(i + glm::vec3(0.0f, 0.0f, 0.0f)), f - glm::vec3(0.0f, 0.0f, 0.0f));
    const float n100 = glm::dot(hash3(i + glm::vec3(1.0f, 0.0f, 0.0f)), f - glm::vec3(1.0f, 0.0f, 0.0f));
    const float n010 = glm::dot(hash3(i + glm::vec3(0.0f, 1.0f, 0.0f)), f - glm::vec3(0.0f, 1.0f, 0.0f));
    const float n110 = glm::dot(hash3(i + glm::vec3(1.0f, 1.0f, 0.0f)), f - glm::vec3(1.0f, 1.0f, 0.0f));
    const float n001 = glm::dot(hash3(i + glm::vec3(0.0f, 0.0f, 1.0f)), f - glm::vec3(0.0f, 0.0f, 1.0f));
    const float n101 = glm::dot(hash3(i + glm::vec3(1.0f, 0.0f, 1.0f)), f - glm::vec3(1.0f, 0.0f, 1.0f));
    const float n011 = glm::dot(hash3(i + glm::vec3(0.0f, 1.0f, 1.0f)), f - glm::vec3(0.0f, 1.0f, 1.0f));
    const float n111 = glm::dot(hash3(i + glm::vec3(1.0f, 1.0f, 1.0f)), f - glm::vec3(1.0f, 1.0f, 1.0f));

    const float nx00 = glm::mix(n000, n100, u.x);
    const float nx10 = glm::mix(n010, n110, u.x);
    const float nx01 = glm::mix(n001, n101, u.x);
    const float nx11 = glm::mix(n011, n111, u.x);
    const float nxy0 = glm::mix(nx00, nx10, u.y);
    const float nxy1 = glm::mix(nx01, nx11, u.y);
    return glm::mix(nxy0, nxy1, u.z);
}

float PlanetProceduralData::perlinNoise(const glm::vec3& p)
{
    const glm::vec3 i = glm::floor(p);
    const glm::vec3 f = glm::fract(p);
    const glm::vec3 u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f);
    constexpr glm::vec3 gradients[12] = {
        glm::vec3( 1.0f,  1.0f,  0.0f), glm::vec3(-1.0f,  1.0f,  0.0f),
        glm::vec3( 1.0f, -1.0f,  0.0f), glm::vec3(-1.0f, -1.0f,  0.0f),
        glm::vec3( 1.0f,  0.0f,  1.0f), glm::vec3(-1.0f,  0.0f,  1.0f),
        glm::vec3( 1.0f,  0.0f, -1.0f), glm::vec3(-1.0f,  0.0f, -1.0f),
        glm::vec3( 0.0f,  1.0f,  1.0f), glm::vec3( 0.0f, -1.0f,  1.0f),
        glm::vec3( 0.0f,  1.0f, -1.0f), glm::vec3( 0.0f, -1.0f, -1.0f)
    };

    const auto gradientAt = [&](const glm::vec3& lattice) {
        const glm::vec3 h = glm::floor(glm::abs(glm::sin(glm::vec3(
            glm::dot(lattice, glm::vec3(127.1f, 311.7f, 74.7f)),
            glm::dot(lattice, glm::vec3(269.5f, 183.3f, 246.1f)),
            glm::dot(lattice, glm::vec3(113.5f, 271.9f, 124.6f))
        )) * 43758.5453f));
        const int index = static_cast<int>(static_cast<std::uint32_t>(h.x + h.y * 7.0f + h.z * 13.0f) % 12u);
        return glm::normalize(gradients[index]);
    };
    const auto corner = [&](float x, float y, float z) {
        const glm::vec3 offset(x, y, z);
        return glm::dot(gradientAt(i + offset), f - offset);
    };

    const float n000 = corner(0.0f, 0.0f, 0.0f);
    const float n100 = corner(1.0f, 0.0f, 0.0f);
    const float n010 = corner(0.0f, 1.0f, 0.0f);
    const float n110 = corner(1.0f, 1.0f, 0.0f);
    const float n001 = corner(0.0f, 0.0f, 1.0f);
    const float n101 = corner(1.0f, 0.0f, 1.0f);
    const float n011 = corner(0.0f, 1.0f, 1.0f);
    const float n111 = corner(1.0f, 1.0f, 1.0f);

    const float nx00 = glm::mix(n000, n100, u.x);
    const float nx10 = glm::mix(n010, n110, u.x);
    const float nx01 = glm::mix(n001, n101, u.x);
    const float nx11 = glm::mix(n011, n111, u.x);
    const float nxy0 = glm::mix(nx00, nx10, u.y);
    const float nxy1 = glm::mix(nx01, nx11, u.y);
    return glm::clamp(glm::mix(nxy0, nxy1, u.z) * 1.45f, -1.0f, 1.0f);
}

float PlanetProceduralData::perlinFbm(const glm::vec3& p, int octaves, float lacunarity, float gain)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        value += amplitude * perlinNoise(p * frequency);
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return value / std::max(maxValue, 0.00001f);
}

float PlanetProceduralData::fbm(const glm::vec3& p, int octaves, float lacunarity, float gain)
{
    // fBM = 多个 octave 的 gradientNoise 叠加；频率逐层升高、振幅逐层降低。
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        value += amplitude * gradientNoise(p * frequency);
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return value / std::max(maxValue, 0.00001f);
}

float PlanetProceduralData::ridgedFbm(const glm::vec3& p,
                                      int octaves,
                                      float lacunarity,
                                      float gain,
                                      float ridgeSharpness)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float maxValue = 0.0f;
    float previous = 1.0f;
    const float sharpness = std::max(ridgeSharpness, 0.1f);

    for (int i = 0; i < octaves; ++i) {
        float ridge = 1.0f - std::abs(gradientNoise(p * frequency));
        ridge = std::pow(glm::clamp(ridge, 0.0f, 1.0f), sharpness);
        value += ridge * amplitude * previous;
        maxValue += amplitude;
        previous = glm::mix(0.68f, 1.0f, ridge);
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return glm::clamp(value / std::max(maxValue, 0.00001f), 0.0f, 1.0f);
}

float PlanetProceduralData::altitudeBandWeight(float startAltitude, float endAltitude)
{
    return 1.0f - glm::smoothstep(startAltitude, endAltitude, 0.0f);
}

float PlanetProceduralData::terrainHeight(const PlanetRenderSettings& settings, const glm::vec3& sphereDir)
{
    // 基础高度合成顺序：大陆轮廓 -> 高地/盆地 -> 山脉/峰顶 -> 海底地貌。
    const glm::vec3 p = sphereDir * settings.terrainNoiseScale;

    const glm::vec3 seaLandP = sphereDir * 1.05f;
    const glm::vec3 seaLandWarp(
        perlinNoise(seaLandP * 0.55f + glm::vec3(3.1f, 0.0f, 0.0f)),
        perlinNoise(seaLandP * 0.55f + glm::vec3(0.0f, 4.7f, 0.0f)),
        perlinNoise(seaLandP * 0.55f + glm::vec3(0.0f, 0.0f, 5.3f))
    );

    const float macroLand = perlinNoise(seaLandP + seaLandWarp * 0.65f);
    const float regionalLand = perlinNoise(seaLandP * 1.85f + seaLandWarp * 0.28f + glm::vec3(6.8f, 2.1f, 11.7f));
    const float coastBand = 1.0f - glm::smoothstep(0.08f, 0.30f, std::abs(macroLand + regionalLand * 0.16f));
    const float coastDetail = perlinNoise(seaLandP * 3.50f + seaLandWarp * 0.16f + glm::vec3(17.6f, 9.4f, 3.2f));
    const float continents = macroLand * 0.84f + regionalLand * 0.16f + coastDetail * coastBand * 0.040f;
    const float baseHeight = continents * 0.44f - 0.012f;
    const float baseRelativeToSea = baseHeight - settings.seaLevelOffset;
    const float coastalLandMask = glm::smoothstep(-0.010f, 0.045f, baseRelativeToSea);
    const float stableLandMask = glm::smoothstep(0.045f, 0.180f, baseRelativeToSea);
    const float continentalShelf = glm::smoothstep(-0.34f, 0.36f, continents);
    const float landCore = stableLandMask;
    const glm::vec3 warp(
        perlinNoise(p * 0.18f + glm::vec3(31.1f, 4.2f, 11.8f)),
        perlinNoise(p * 0.18f + glm::vec3(9.6f, 27.4f, 3.2f)),
        perlinNoise(p * 0.18f + glm::vec3(5.1f, 8.7f, 24.6f))
    );
    const float highlandRaw = perlinNoise(p * 0.22f + warp * 0.20f + glm::vec3(18.2f, 3.8f, 27.6f)) * 0.5f + 0.5f;
    float highlandShoulder = glm::smoothstep(0.50f, 0.78f, highlandRaw) * continentalShelf * landCore;
    float highlandCore = glm::smoothstep(0.68f, 0.90f, highlandRaw) * continentalShelf * landCore;
    highlandShoulder = std::pow(glm::clamp(highlandShoulder, 0.0f, 1.0f), 1.85f);
    highlandCore = std::pow(glm::clamp(highlandCore, 0.0f, 1.0f), 2.40f);
    const float highlandMask = glm::clamp(highlandShoulder * 0.65f + highlandCore, 0.0f, 1.0f);
    const float highlandVariation = perlinNoise(p * 0.55f + warp * 0.18f + glm::vec3(7.4f, 22.1f, 4.3f)) * 0.5f + 0.5f;

    float basinMask = perlinNoise(p * 0.34f + warp * 0.12f + glm::vec3(8.7f, 2.4f, 13.1f)) * 0.5f + 0.5f;
    basinMask = glm::smoothstep(0.70f, 0.92f, basinMask) * stableLandMask * (1.0f - highlandCore * 0.85f);

    float h = baseHeight;
    h += highlandShoulder * stableLandMask * (0.026f + highlandVariation * 0.009f);
    h += highlandCore * stableLandMask * (0.070f + highlandVariation * 0.024f);
    h -= basinMask * (0.012f + (1.0f - highlandVariation) * 0.008f);
    h = (h < 0.0f ? -1.0f : 1.0f) * std::pow(std::abs(h), 1.15f);

    const float relativeToSea = h - settings.seaLevelOffset;
    const float landUpliftMask = coastalLandMask * continentalShelf;
    if (landUpliftMask > 0.0f) {
        const float lowlandMask = coastalLandMask * (1.0f - glm::smoothstep(0.16f, 0.34f, baseRelativeToSea));
        const float plateauMask = highlandMask * glm::smoothstep(0.10f, 0.34f, relativeToSea);
        const float continentalInterior = glm::smoothstep(0.18f, 0.70f, continentalShelf);
        const float uplift =
            0.018f * lowlandMask +
            0.018f * highlandShoulder * highlandVariation * continentalInterior +
            0.042f * highlandCore;
        h += uplift * landUpliftMask;
    }

    const float signedWaterDepth = settings.seaLevelOffset - h;
    const float oceanMask = glm::smoothstep(0.0f, 0.045f, signedWaterDepth);
    if (oceanMask > 0.0f) {
        // 海面以下继续塑造大陆架、深海盆地、中洋脊和海沟。
        const float shoreRamp = glm::smoothstep(0.0f, 0.075f, signedWaterDepth);
        const float shelfMask = glm::smoothstep(0.015f, 0.20f, signedWaterDepth);
        const float basinMask = glm::smoothstep(0.16f, 0.56f, signedWaterDepth);
        const float abyssalMask = glm::smoothstep(0.40f, 0.90f, signedWaterDepth);
        const float basinNoise = perlinNoise(p * 0.72f + warp * 0.28f + glm::vec3(21.4f, 6.2f, 13.8f)) * 0.5f + 0.5f;
        const float ridgeNoise = 1.0f - std::abs(gradientNoise(p * 5.6f + warp * 2.2f + glm::vec3(2.6f, 19.1f, 8.4f)));
        const float trenchNoise = 1.0f - std::abs(gradientNoise(p * 3.4f + warp * 1.6f + glm::vec3(31.7f, 4.9f, 11.2f)));
        const float oceanFloorDrop =
            0.06f * oceanMask +
            0.16f * shelfMask +
            0.22f * basinMask +
            0.14f * abyssalMask +
            basinNoise * 0.12f * basinMask;
        const float midOceanRidge = std::pow(glm::clamp(ridgeNoise, 0.0f, 1.0f), 3.2f) * basinMask * 0.08f;
        const float trenchDrop = std::pow(glm::clamp(trenchNoise, 0.0f, 1.0f), 5.0f) * abyssalMask * 0.12f;
        h -= oceanFloorDrop * shoreRamp;
        h += midOceanRidge;
        h -= trenchDrop;
    }
    return h;
}

PlanetProceduralData::PlanetSample PlanetProceduralData::samplePlanetBase(const PlanetRenderSettings& settings,
                                                                          const glm::vec3& sphereDir)
{
    return samplePlanetBase(settings, sphereDir, terrainHeight(settings, glm::normalize(sphereDir)));
}

PlanetProceduralData::PlanetSample PlanetProceduralData::samplePlanetBase(const PlanetRenderSettings& settings,
                                                                          const glm::vec3& sphereDir,
                                                                          float height)
{
    // 基础采样只依赖当前高度和球面方向，供多轮生成阶段重复使用。
    const glm::vec3 n = glm::normalize(sphereDir);
    PlanetSample sample;
    sample.height = height;

    const float signedWaterDepth = (settings.seaLevelOffset - height) * settings.terrainHeightScale;
    sample.waterDepth = std::max(signedWaterDepth, 0.0f);

    const float shoreWidth = std::max(settings.oceanShoreBlendWidth, 0.001f);
    sample.shoreMask = 1.0f - glm::smoothstep(0.0f, shoreWidth, std::abs(signedWaterDepth));
    sample.temperature = temperature(settings, n, height);
    sample.moisture = moisture(n, sample.shoreMask);
    return sample;
}

float PlanetProceduralData::temperature(const PlanetRenderSettings& settings, const glm::vec3& sphereDir, float height)
{
    // 温度 = 纬度主导 + 海拔降温 + 少量 fBM 扰动。
    const float latitude01 = std::abs(sphereDir.y);
    const float latitudeTemperature = 1.0f - latitude01;
    const float heightCooling = std::max(height - settings.seaLevelOffset, 0.0f) * 0.35f;
    const float temperatureNoise = fbm(sphereDir * 3.0f + glm::vec3(8.1f, 2.7f, 5.4f), 4, 2.0f, 0.5f) * 0.12f;
    return glm::clamp(latitudeTemperature - heightCooling + temperatureNoise, 0.0f, 1.0f);
}

float PlanetProceduralData::moisture(const glm::vec3& sphereDir, float shoreMask)
{
    // 湿度 = 大尺度噪声 + 海岸加湿 + 轻微纬度修正；侵蚀后还会被水文再次修正。
    const float moistureNoise = fbm(sphereDir * 4.0f + glm::vec3(1.2f, 9.3f, 4.8f), 5, 2.0f, 0.5f) * 0.5f + 0.5f;
    const float shoreMoisture = shoreMask * 0.35f;
    const float latitudeMoisture = 1.0f - std::abs(sphereDir.y) * 0.25f;
    return glm::clamp(moistureNoise * 0.65f + shoreMoisture + latitudeMoisture * 0.15f, 0.0f, 1.0f);
}
