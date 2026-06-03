#include "PlanetProceduralData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <numeric>

#include "Instumentor/InstrumentationTimer.hpp"
#include "PlanetTerrainGenerator.h"

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
// 程序化缓存文件头常量，magic 和 version 必须与读写逻辑保持一致。
constexpr char kProceduralCacheMagic[8] = { 'P', 'W', 'C', 'A', 'C', 'H', 'E', '9' };
constexpr std::uint32_t kProceduralCacheVersion = 117;
constexpr int kTerrainChunkMeshResolution = 24;
constexpr int kTerrainChunkMinDepth = 2;
constexpr int kTerrainChunkMaxDepth = 6;
constexpr int kFinalizeChunkProgressSteps = 256;
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

// 基于 value-noise fBm 计算“海岸遮蔽度”，用于区分开阔海岸与内湾。
// 遮蔽度越高，越倾向生成平缓、湿润的近海地貌分布。
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

// 大尺度气候分区噪声：用于打散纬度条带，让生物群落形成“区域块状”分布。
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

// 将地形高度、水文、温湿度、坡度等因素融合为 8 类 biome 权重。
// 这些权重会被打包成两个 vec4 层，供 shader 进行地表材质混合。
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
    // 先写入全局元数据，再按 face 顺序写入每层标量/向量场。
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
    // 先校验缓存版本和分辨率，避免加载不兼容或损坏的数据。
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
    buildTerrainFeatureSegments(settings_);
    buildTerrainChunks(settings_);
    generated_ = true;
    return true;
}

PlanetGlobalHeightField PlanetProceduralData::globalHeightField() const
{
    // 将内部 FaceData 拍平成全局高度场结构，供调试/导出/LOD 分析复用。
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

    generateDemPrototype(settings, progressCallback);
    return;

    const int erosionIterations = std::clamp(settings.erosionIterations, 0, 256);
    const float erosionStrength = std::max(settings.erosionStrength, 0.0f);
    const float thermalStrength = std::max(settings.erosionThermalStrength, 0.0f);
    const bool erosionActive = erosionIterations > 0 && (erosionStrength > 0.0f || thermalStrength > 0.0f);
    const int thermalIterations = erosionActive && thermalStrength > 0.0f
        ? std::clamp(erosionIterations / 3, 1, 80)
        : 0;
    std::array<int, static_cast<std::size_t>(GenerationModule::Count)> moduleTotals{};
    std::array<int, static_cast<std::size_t>(GenerationModule::Count)> moduleCompleted{};
    moduleTotals[static_cast<std::size_t>(GenerationModule::BaseTerrain)] = resolution_ * 6 + 1;
    moduleTotals[static_cast<std::size_t>(GenerationModule::InitialClimate)] = resolution_ * 6 + 1;
    moduleTotals[static_cast<std::size_t>(GenerationModule::InitialBiomes)] = resolution_ * 6 + 2;
    moduleTotals[static_cast<std::size_t>(GenerationModule::BiomeTerrain)] = 0;
    moduleTotals[static_cast<std::size_t>(GenerationModule::Erosion)] = 0;
    moduleTotals[static_cast<std::size_t>(GenerationModule::FinalClimate)] = resolution_ * 6 + 1;
    moduleTotals[static_cast<std::size_t>(GenerationModule::FinalBiomes)] = resolution_ * 6 + 2;
    moduleTotals[static_cast<std::size_t>(GenerationModule::MeshPlanning)] = resolution_ * 6 + 1;
    moduleTotals[static_cast<std::size_t>(GenerationModule::Finalize)] = 1 + 6 + 1 + kFinalizeChunkProgressSteps + 1;

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
    const auto finishModuleProgress = [&](GenerationModule module, const char* status) {
        activeModule = module;
        const std::size_t moduleIndex = static_cast<std::size_t>(module);
        const int remainingModuleSteps = std::max(moduleTotals[moduleIndex] - moduleCompleted[moduleIndex], 0);
        completedSteps = std::min(completedSteps + remainingModuleSteps, totalSteps);
        moduleCompleted[moduleIndex] = moduleTotals[moduleIndex];
        reportProgress(status);
    };

    reportProgress("Preparing terrain buffers");

    // Build the base heightfield for all six faces.
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
            advanceModuleProgress(GenerationModule::BaseTerrain, "Generating base terrain");
        }
    }

    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::BaseTerrain, "Blending base terrain seams");

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

    computeWaterClimateFields(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::FinalClimate, status);
    });

    fixCubeFaceSeams();
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

    buildTerrainFeatureSegments(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::Finalize, status);
    });
    buildTerrainChunks(settings, [&](const char* status) {
        advanceModuleProgress(GenerationModule::Finalize, status);
    });
    finishModuleProgress(GenerationModule::Finalize, "Generation complete");
    generated_ = true;
    reportProgress("Generation complete");
}

void PlanetProceduralData::generateDemPrototype(const PlanetRenderSettings& settings,
                                                const ProgressCallback& progressCallback)
{
    PROFILE_SCOPE("Generate DEM Prototype");
    const int totalSteps = 11;
    int completedSteps = 0;
    GenerationModule activeModule = GenerationModule::BaseTerrain;
    const auto report = [&](GenerationModule module, const char* status) {
        activeModule = module;
        if (progressCallback) {
            progressCallback(GenerationProgress{
                completedSteps,
                totalSteps,
                activeModule,
                completedSteps,
                totalSteps,
                status
            });
        }
    };
    const auto advance = [&](GenerationModule module, const char* status) {
        completedSteps = std::min(completedSteps + 1, totalSteps);
        report(module, status);
    };

    report(GenerationModule::BaseTerrain, "Preparing DEM buffers");

    terrainChunks_.clear();
    terrainFeatureSegments_.clear();
    const int n = resolution_;
    const std::size_t faceCellCount = static_cast<std::size_t>(n * n);
    const std::size_t globalCellCount = faceCellCount * 6;
    if (n <= 0) {
        generated_ = true;
        return;
    }

    for (FaceData& faceData : faces_) {
        faceData.resolution = n;
        faceData.height.assign(faceCellCount, 0.0f);
        faceData.waterDepth.assign(faceCellCount, 0.0f);
        faceData.shoreMask.assign(faceCellCount, 0.0f);
        faceData.erosionMask.assign(faceCellCount, 0.0f);
        faceData.channelMask.assign(faceCellCount, 0.0f);
        faceData.flowMask.assign(faceCellCount, 0.0f);
        faceData.wearMask.assign(faceCellCount, 0.0f);
        faceData.depositionMask.assign(faceCellCount, 0.0f);
        faceData.temperature.assign(faceCellCount, 0.0f);
        faceData.moisture.assign(faceCellCount, 0.0f);
        faceData.regionId.assign(faceCellCount, 0.0f);
        faceData.featureMask.assign(faceCellCount, 0.0f);
        faceData.meshDensity.assign(faceCellCount, 0.0f);
        faceData.geometricError.assign(faceCellCount, 0.0f);
        faceData.biomeWeightA.assign(faceCellCount, glm::vec4(0.0f));
        faceData.biomeWeightB.assign(faceCellCount, glm::vec4(0.0f));
        faceData.domainWeight.assign(faceCellCount, glm::vec4(0.0f));
    }

    const auto globalIndex = [faceCellCount](int faceIndex, std::size_t localIndex) {
        return static_cast<std::size_t>(faceIndex) * faceCellCount + localIndex;
    };
    const auto splitGlobalIndex = [faceCellCount](std::size_t global, int& faceIndex, std::size_t& localIndex) {
        faceIndex = static_cast<int>(global / faceCellCount);
        localIndex = global % faceCellCount;
    };
    const auto sampleDir = [&](int faceIndex, int x, int y) {
        const glm::vec2 uv(
            (static_cast<float>(x) + 0.5f) / static_cast<float>(n),
            (static_cast<float>(y) + 0.5f) / static_cast<float>(n)
        );
        return cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], uv);
    };
    std::vector<float> uplift(globalCellCount, 0.0f);
    std::vector<float> landMask(globalCellCount, 0.0f);
    std::vector<float> preErosionHeight(globalCellCount, 0.0f);
    const float seaLevel = settings.seaLevelOffset;
    const float noiseScale = std::max(settings.terrainNoiseScale, 0.05f);

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const std::size_t local = static_cast<std::size_t>(y * n + x);
                const std::size_t global = globalIndex(faceIndex, local);
                const glm::vec3 dir = sampleDir(faceIndex, x, y);

                const float macro = fbm(dir * (1.05f * noiseScale + 0.25f), 5, 2.03f, 0.52f);
                const float regional = fbm(dir * (2.10f * noiseScale + 0.55f) + glm::vec3(13.1f, -2.7f, 5.9f), 4, 2.08f, 0.50f);
                const float latitude = 1.0f - std::abs(dir.y);
                const float continentCore = macro * 0.70f + regional * 0.22f + latitude * 0.08f;
                const float oldContinent = glm::smoothstep(-0.17f, 0.24f, continentCore);
                const float oldInterior = glm::smoothstep(0.20f, 0.78f, oldContinent);
                const float continentPatch = glm::smoothstep(-0.22f, 0.36f, macro * 0.60f + regional * 0.18f);
                const float continent = glm::clamp(oldContinent * 0.88f + continentPatch * oldInterior * 0.12f, 0.0f, 1.0f);
                const float coastShelf = glm::smoothstep(0.10f, 0.68f, continent);
                const float provinceNoise = fbm(dir * (1.85f * noiseScale + 0.35f) + glm::vec3(-9.0f, 3.0f, 12.0f), 5, 2.03f, 0.52f);
                const float rangeField = ridgedFbm(dir * (2.65f * noiseScale + 0.55f) + glm::vec3(4.0f, -8.0f, 11.0f), 5, 2.06f, 0.50f, 1.55f);
                const float branchField = ridgedFbm(dir * (5.10f * noiseScale + 1.10f) + glm::vec3(-2.0f, 6.0f, -5.0f), 4, 2.12f, 0.47f, 1.85f);
                const float scatteredRanges = glm::smoothstep(0.52f, 0.90f, rangeField * 0.72f + branchField * 0.28f + provinceNoise * 0.12f);
                const float province = glm::smoothstep(0.38f, 0.88f, scatteredRanges * 0.76f + provinceNoise * 0.16f + oldInterior * 0.08f) * oldInterior;
                const float broadRelief = fbm(dir * (2.35f * noiseScale + 0.45f) + glm::vec3(-4.0f, 8.0f, 2.0f), 5, 2.0f, 0.50f);
                const float midRelief = fbm(dir * (7.8f * noiseScale + 1.3f) + glm::vec3(2.5f, -11.0f, 4.0f), 4, 2.12f, 0.48f);
                const float ridgeSeed = ridgedFbm(dir * (4.2f * noiseScale + 0.9f), 5, 2.05f, 0.50f, 2.15f);
                const float fineRidgeSeed = ridgedFbm(dir * (9.4f * noiseScale + 1.8f) + glm::vec3(5.0f, 1.0f, -3.0f), 3, 2.10f, 0.44f, 2.55f);
                const float ridgeRelief = glm::smoothstep(0.54f, 0.92f, ridgeSeed) * province;
                const float fineRidgeRelief = glm::smoothstep(0.64f, 0.96f, fineRidgeSeed) * province;
                const float foothill = glm::smoothstep(0.22f, 0.86f, province) * (0.5f + broadRelief * 0.5f);

                float height = seaLevel - 0.180f + continent * 0.305f;
                height += coastShelf * broadRelief * 0.040f;
                height += coastShelf * midRelief * 0.014f;
                height += province * 0.060f;
                height += ridgeRelief * 0.075f;
                height += fineRidgeRelief * 0.012f;
                height += foothill * 0.034f;
                if (height > seaLevel) {
                    const float highland = height - seaLevel;
                    const float peakBoost = glm::smoothstep(0.060f, 0.320f, highland);
                    height = seaLevel + highland * glm::mix(1.12f, 1.72f, peakBoost);
                }
                if (height < seaLevel) {
                    height -= (seaLevel - height) * 0.58f;
                }

                faceData.height[local] = height;
                uplift[global] = glm::clamp(province * 0.54f + ridgeRelief * 0.34f + fineRidgeRelief * 0.22f, 0.0f, 1.0f);
                landMask[global] = glm::smoothstep(seaLevel - 0.006f, seaLevel + 0.020f, height);
                preErosionHeight[global] = height;
            }
        }
    }
    advance(GenerationModule::BaseTerrain, "Built continents and uplift field");

    fixCubeFaceSeams();
    advance(GenerationModule::BaseTerrain, "Blended DEM seams");

    const auto copyHeightsToVector = [&]() {
        std::vector<float> heights(globalCellCount, 0.0f);
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            const FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            for (std::size_t local = 0; local < faceCellCount; ++local) {
                heights[globalIndex(faceIndex, local)] = faceData.height[local];
            }
        }
        return heights;
    };
    const auto writeVectorToHeights = [&](const std::vector<float>& heights) {
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
            for (std::size_t local = 0; local < faceCellCount; ++local) {
                faceData.height[local] = heights[globalIndex(faceIndex, local)];
            }
        }
    };

    const int erosionPasses = std::clamp(settings.erosionIterations / 24, 2, 8);
    const float erosionStrength = std::max(settings.erosionStrength, 0.0f);
    const float thermalStrength = std::max(settings.erosionThermalStrength, 0.0f);
    std::vector<float> finalDrainage(globalCellCount, 0.0f);
    std::vector<float> finalSlope(globalCellCount, 0.0f);
    std::vector<float> finalWear(globalCellCount, 0.0f);

    for (int pass = 0; pass < erosionPasses; ++pass) {
        std::vector<float> heights = copyHeightsToVector();
        std::vector<std::array<int, 8>> receivers(globalCellCount);
        std::vector<std::array<float, 8>> receiverWeights(globalCellCount);
        std::vector<float> downSlope(globalCellCount, 0.0f);
        std::vector<float> drainage(globalCellCount, 0.0f);
        std::vector<std::size_t> order(globalCellCount);
        std::iota(order.begin(), order.end(), std::size_t{0});
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            return heights[a] > heights[b];
        });
        for (std::size_t global = 0; global < globalCellCount; ++global) {
            receivers[global].fill(-1);
            receiverWeights[global].fill(0.0f);
        }

        for (std::size_t global = 0; global < globalCellCount; ++global) {
            int faceIndex = 0;
            std::size_t local = 0;
            splitGlobalIndex(global, faceIndex, local);
            const int x = static_cast<int>(local % static_cast<std::size_t>(n));
            const int y = static_cast<int>(local / static_cast<std::size_t>(n));
            const float h = heights[global];
            drainage[global] = h > seaLevel ? 1.0f + uplift[global] * 0.35f : 0.0f;

            float bestSlope = 0.0f;
            float weightSum = 0.0f;
            int receiverCount = 0;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (ox == 0 && oy == 0) {
                        continue;
                    }
                    const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                    const std::size_t neighborGlobal = globalIndex(neighbor.face, neighbor.index);
                    const float drop = h - heights[neighborGlobal];
                    const float distance = (ox != 0 && oy != 0) ? 1.41421356f : 1.0f;
                    const float slope = drop / distance;
                    if (slope > bestSlope) {
                        bestSlope = slope;
                    }
                    if (slope > 0.0f && receiverCount < 8) {
                        const float rawWeight = std::pow(slope + 1.0e-6f, 1.18f);
                        receivers[global][static_cast<std::size_t>(receiverCount)] = static_cast<int>(neighborGlobal);
                        receiverWeights[global][static_cast<std::size_t>(receiverCount)] = rawWeight;
                        weightSum += rawWeight;
                        ++receiverCount;
                    }
                }
            }
            if (weightSum > 0.0f) {
                const float invWeightSum = 1.0f / weightSum;
                for (int i = 0; i < receiverCount; ++i) {
                    receiverWeights[global][static_cast<std::size_t>(i)] *= invWeightSum;
                }
            }
            downSlope[global] = bestSlope;
        }

        for (std::size_t global : order) {
            for (int i = 0; i < 8; ++i) {
                const int receiver = receivers[global][static_cast<std::size_t>(i)];
                const float weight = receiverWeights[global][static_cast<std::size_t>(i)];
                if (receiver >= 0 && weight > 0.0f) {
                    drainage[static_cast<std::size_t>(receiver)] += drainage[global] * weight;
                }
            }
        }

        const float maxDrainage = std::max(*std::max_element(drainage.begin(), drainage.end()), 1.0f);
        std::vector<float> next = heights;
        for (std::size_t global = 0; global < globalCellCount; ++global) {
            const float h = heights[global];
            const float land = glm::smoothstep(seaLevel - 0.004f, seaLevel + 0.030f, h);
            if (land <= 0.0f) {
                continue;
            }
            const float flow = drainage[global] / maxDrainage;
            const float channel = glm::smoothstep(0.006f, 0.20f, std::pow(flow, 0.34f));
            const float slope = glm::clamp(downSlope[global] * 56.0f, 0.0f, 1.0f);
            const float streamPower = std::pow(glm::clamp(flow * 30.0f, 0.0f, 1.0f), 0.50f)
                                    * std::pow(slope, 0.72f)
                                    * land;
            const float carve = glm::min(0.140f * erosionStrength * streamPower, downSlope[global] * 0.48f);
            const float deposition = 0.0034f * erosionStrength * channel * (1.0f - slope) * land;
            next[global] += deposition - carve;
            finalDrainage[global] = glm::max(finalDrainage[global], channel);
            finalSlope[global] = glm::max(finalSlope[global], slope);
            finalWear[global] = glm::max(finalWear[global], glm::clamp(carve * 80.0f, 0.0f, 1.0f));
        }
        writeVectorToHeights(next);
        fixCubeFaceSeams();
    }
    advance(GenerationModule::Erosion, "Carved drainage network");

    const int diffusionPasses = std::clamp(settings.erosionIterations / 32, 2, 8);
    for (int pass = 0; pass < diffusionPasses; ++pass) {
        std::vector<float> heights = copyHeightsToVector();
        std::vector<float> next = heights;
        for (std::size_t global = 0; global < globalCellCount; ++global) {
            int faceIndex = 0;
            std::size_t local = 0;
            splitGlobalIndex(global, faceIndex, local);
            const int x = static_cast<int>(local % static_cast<std::size_t>(n));
            const int y = static_cast<int>(local / static_cast<std::size_t>(n));
            const float h = heights[global];
            if (h <= seaLevel - 0.005f) {
                continue;
            }
            float sum = 0.0f;
            int count = 0;
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    if (ox == 0 && oy == 0) {
                        continue;
                    }
                    const CellRef neighbor = neighborCell(faceIndex, x + ox, y + oy, n);
                    sum += heights[globalIndex(neighbor.face, neighbor.index)];
                    ++count;
                }
            }
            const float average = sum / static_cast<float>(std::max(count, 1));
            const float localSlope = glm::clamp(std::abs(h - average) * 38.0f, 0.0f, 1.0f);
            const float blend = thermalStrength * (0.10f + localSlope * 0.65f) * (1.0f - finalDrainage[global] * 0.55f);
            next[global] = glm::mix(h, average, glm::clamp(blend, 0.0f, 0.18f));
        }
        writeVectorToHeights(next);
        fixCubeFaceSeams();
    }
    advance(GenerationModule::Erosion, "Diffused hillslopes");

    computeWaterClimateFields(settings, [&](const char*) {});
    advance(GenerationModule::FinalClimate, "Computed water and climate");

    minHeight_ = std::numeric_limits<float>::max();
    maxHeight_ = std::numeric_limits<float>::lowest();
    maxWaterDepth_ = 0.0f;
    std::size_t waterSamples = 0;
    std::size_t shoreSamples = 0;
    std::size_t totalSamples = 0;
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const std::size_t local = static_cast<std::size_t>(y * n + x);
                const std::size_t global = globalIndex(faceIndex, local);
                const float h = faceData.height[local];
                const float waterDepth = glm::max(seaLevel - h, 0.0f);
                const float land = waterDepth <= 0.0f ? 1.0f : 0.0f;
                const float upliftValue = uplift[global];
                const float channel = finalDrainage[global] * land;
                const float slope = glm::max(finalSlope[global], faceData.erosionMask[local]);
                const float wear = finalWear[global] * land;
                const float delta = std::max(preErosionHeight[global] - h, 0.0f);

                faceData.waterDepth[local] = waterDepth;
                faceData.channelMask[local] = channel;
                faceData.flowMask[local] = glm::smoothstep(0.08f, 0.65f, channel);
                faceData.wearMask[local] = wear;
                faceData.depositionMask[local] = glm::smoothstep(0.003f, 0.018f, delta) * (1.0f - slope) * land;
                faceData.erosionMask[local] = glm::clamp(slope * land, 0.0f, 1.0f);
                const float upliftMesh = upliftValue * upliftValue;
                faceData.featureMask[local] = glm::clamp(
                    upliftMesh * 0.34f + channel * 0.34f + slope * 0.24f + faceData.shoreMask[local] * 0.08f,
                    0.0f,
                    0.76f
                );
                faceData.meshDensity[local] = glm::clamp(0.10f + upliftMesh * 0.24f + slope * 0.24f + channel * 0.20f, 0.0f, 0.58f);
                faceData.geometricError[local] = glm::clamp(0.08f + upliftMesh * 0.20f + slope * 0.30f + channel * 0.16f, 0.0f, 0.62f);
                faceData.regionId[local] = waterDepth > 0.0f ? 5.0f : 1.0f;
                faceData.biomeWeightA[local] = glm::vec4(0.0f, land, 0.0f, 0.0f);
                faceData.biomeWeightB[local] = glm::vec4(slope * land, 0.0f, 0.0f, waterDepth > 0.0f ? 1.0f : 0.0f);
                faceData.domainWeight[local] = glm::vec4(upliftValue, channel, slope, land);

                minHeight_ = std::min(minHeight_, h);
                maxHeight_ = std::max(maxHeight_, h);
                maxWaterDepth_ = std::max(maxWaterDepth_, waterDepth);
                waterSamples += waterDepth > 0.0f ? 1 : 0;
                shoreSamples += faceData.shoreMask[local] > 0.05f ? 1 : 0;
                ++totalSamples;
            }
        }
    }
    waterCoverage_ = totalSamples > 0 ? static_cast<float>(waterSamples) / static_cast<float>(totalSamples) : 0.0f;
    shoreCoverage_ = totalSamples > 0 ? static_cast<float>(shoreSamples) / static_cast<float>(totalSamples) : 0.0f;
    advance(GenerationModule::MeshPlanning, "Prepared DEM debug layers");

    fixCubeFaceSeams();
    advance(GenerationModule::Finalize, "Blended final DEM seams");

    terrainFeatureSegments_.clear();
    advance(GenerationModule::Finalize, "Skipped DEM feature overlays");

    buildTerrainChunks(settings, [&](const char*) {});
    advance(GenerationModule::Finalize, "Built lightweight terrain chunks");

    generated_ = true;
    completedSteps = totalSteps;
    report(GenerationModule::Finalize, "Generation complete");
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

    // 对标量场做立方体面缝合：把跨 face 的边界采样统一到相同值。
    // neighborCell 会把越界 UV 映射到相邻面，覆盖 12 条棱及角点附近区域。
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
    // 对 vec4 层执行同样的缝合（如 biome/domain 权重）。
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

void PlanetProceduralData::buildTerrainFeatureSegments(const PlanetRenderSettings& settings,
                                                       const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Build Terrain Feature Segments");
    terrainFeatureSegments_.clear();
    if (resolution_ <= 2) {
        return;
    }

    if (advanceProgress) {
        advanceProgress("Building terrain feature segments");
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

void PlanetProceduralData::buildTerrainChunks(const PlanetRenderSettings& settings,
                                              const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Build Offline Terrain Chunks");
    terrainChunks_.clear();
    if (resolution_ <= 1) {
        return;
    }
    int effectiveMaxDepth = 3;
    constexpr int kDemUniformChunkMaxDepth = 5;
    for (int size = std::max(resolution_, 1); size > 16 && effectiveMaxDepth < kDemUniformChunkMaxDepth; size /= 2) {
        ++effectiveMaxDepth;
    }
    const int effectiveMinDepth = effectiveMaxDepth;

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

    int emittedTinChunkCount = 0;
    constexpr int kMaxDemTinChunks = 0;
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
        const bool wantsTinMesh = node.depth >= 4
                               && (maxHeight > settings.seaLevelOffset + 0.130f
                                   || featureMask > 0.42f
                                   || geometricError > 0.42f
                                   || meshDensity > 0.44f);
        const bool useTinMesh = false && wantsTinMesh && emittedTinChunkCount < kMaxDemTinChunks;
        if (useTinMesh) {
            ++emittedTinChunkCount;
        }

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
        localPoints.reserve(224);
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
        constraintEdges.reserve(96);

        for (int i = 0; i <= kTerrainChunkMeshResolution; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kTerrainChunkMeshResolution);
            addLocalPoint(glm::vec2(t, 0.0f));
            addLocalPoint(glm::vec2(t, 1.0f));
            addLocalPoint(glm::vec2(0.0f, t));
            addLocalPoint(glm::vec2(1.0f, t));
        }
        for (int y = 4; y < kTerrainChunkMeshResolution; y += 4) {
            for (int x = 4; x < kTerrainChunkMeshResolution; x += 4) {
                addLocalPoint(glm::vec2(static_cast<float>(x) / static_cast<float>(kTerrainChunkMeshResolution),
                                        static_cast<float>(y) / static_cast<float>(kTerrainChunkMeshResolution)));
            }
        }

        if (useTinMesh) {
            constexpr int kFeaturePointSamples = 8;
            for (int y = 0; y < kFeaturePointSamples; ++y) {
                for (int x = 0; x < kFeaturePointSamples; ++x) {
                    const glm::vec2 local((static_cast<float>(x) + 0.5f) / static_cast<float>(kFeaturePointSamples),
                                          (static_cast<float>(y) + 0.5f) / static_cast<float>(kFeaturePointSamples));
                    const glm::vec2 uv = node.uvMin + local * node.uvSize;
                    const float step = glm::max(1.0f / static_cast<float>(resolution_),
                                                node.uvSize.x / static_cast<float>(kTerrainChunkMeshResolution * 2));
                    const glm::vec2 uvL(glm::max(uv.x - step, 0.0f), uv.y);
                    const glm::vec2 uvR(glm::min(uv.x + step, 1.0f), uv.y);
                    const glm::vec2 uvD(uv.x, glm::max(uv.y - step, 0.0f));
                    const glm::vec2 uvU(uv.x, glm::min(uv.y + step, 1.0f));
                    const float centerHeight = sampleFaceLayerBilinear(face.height, resolution_, uv);
                    const float dx = sampleFaceLayerBilinear(face.height, resolution_, uvR) - sampleFaceLayerBilinear(face.height, resolution_, uvL);
                    const float dy = sampleFaceLayerBilinear(face.height, resolution_, uvU) - sampleFaceLayerBilinear(face.height, resolution_, uvD);
                    const float dxx = sampleFaceLayerBilinear(face.height, resolution_, glm::clamp(uv + glm::vec2(step * 1.5f, 0.0f), glm::vec2(0.0f), glm::vec2(0.999999f)))
                                    - 2.0f * centerHeight
                                    + sampleFaceLayerBilinear(face.height, resolution_, glm::clamp(uv - glm::vec2(step * 1.5f, 0.0f), glm::vec2(0.0f), glm::vec2(0.999999f)));
                    const float dyy = sampleFaceLayerBilinear(face.height, resolution_, glm::clamp(uv + glm::vec2(0.0f, step * 1.5f), glm::vec2(0.0f), glm::vec2(0.999999f)))
                                    - 2.0f * centerHeight
                                    + sampleFaceLayerBilinear(face.height, resolution_, glm::clamp(uv - glm::vec2(0.0f, step * 1.5f), glm::vec2(0.0f), glm::vec2(0.999999f)));
                    const float slope = glm::clamp(std::sqrt(dx * dx + dy * dy) * 7.5f, 0.0f, 1.0f);
                    const float curvature = glm::clamp(std::abs(dxx) + std::abs(dyy), 0.0f, 1.0f);
                    const float feature = sampleFeatureWeight(uv);
                    const float density = sampleFaceLayerBilinear(face.meshDensity, resolution_, uv);
                    const float error = sampleFaceLayerBilinear(face.geometricError, resolution_, uv);
                    const float complexity = glm::clamp(slope * 0.48f + curvature * 0.20f + feature * 0.34f + density * 0.22f + error * 0.18f, 0.0f, 1.0f);
                    const float score = complexity;
                    if (score < 0.22f) {
                        continue;
                    }

                    const float hash = glm::fract(std::sin(glm::dot(uv, glm::vec2(127.1f, 311.7f))) * 43758.5453f);
                    const float hashB = glm::fract(std::sin(glm::dot(uv, glm::vec2(269.5f, 183.3f))) * 24634.6345f);
                    const glm::vec2 jitter(hash - 0.5f, hashB - 0.5f);
                    const float jitterScale = 0.18f / static_cast<float>(kFeaturePointSamples);
                    addLocalPoint(glm::clamp(local + jitter * jitterScale, glm::vec2(0.02f), glm::vec2(0.98f)));

                    if (complexity > 0.48f) {
                        const glm::vec2 ridgeDir(dx, dy);
                        const glm::vec2 ridgePerp = glm::length(ridgeDir) > 1.0e-5f
                            ? glm::normalize(glm::vec2(-ridgeDir.y, ridgeDir.x))
                            : glm::vec2(0.0f, 1.0f);
                        addLocalPoint(glm::clamp(local + ridgePerp * 0.032f, glm::vec2(0.02f), glm::vec2(0.98f)));
                        addLocalPoint(glm::clamp(local - ridgePerp * 0.032f, glm::vec2(0.02f), glm::vec2(0.98f)));
                    }
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

    int chunkProgressTicks = 0;
    int processedNodes = 0;
    const int maxQuadtreeNodes =
        6 * ((1 << ((effectiveMaxDepth + 1) * 2)) - 1) / 3;
    const auto reportChunkProgress = [&]() {
        if (!advanceProgress || maxQuadtreeNodes <= 0) {
            return;
        }

        const int targetTicks = glm::clamp(
            static_cast<int>(
                (static_cast<long long>(processedNodes) * kFinalizeChunkProgressSteps) / maxQuadtreeNodes
            ),
            0,
            kFinalizeChunkProgressSteps
        );
        while (chunkProgressTicks < targetTicks) {
            ++chunkProgressTicks;
            advanceProgress("Building offline terrain chunks");
        }
    };

    while (!stack.empty()) {
        const PendingNode node = stack.back();
        stack.pop_back();
        ++processedNodes;

        float meshDensity = 0.0f;
        float geometricError = 0.0f;
        float featureMask = 0.0f;
        float minHeight = 0.0f;
        float maxHeight = 0.0f;
        bool hasWater = false;
        bool hasShore = false;
        sampleNodeStats(node, meshDensity, geometricError, featureMask, minHeight, maxHeight, hasWater, hasShore);

        const float mountainBoost = glm::smoothstep(0.12f, 0.48f, featureMask + meshDensity * 0.35f + geometricError * 0.28f);
        const float splitScore = meshDensity * 0.50f
                               + geometricError * 1.25f
                               + featureMask * 0.65f
                               + mountainBoost * 0.48f
                               + (hasShore ? 0.28f : 0.0f);
        const float depthBias = static_cast<float>(node.depth) * 0.13f;
        const bool deepOcean = hasWater && !hasShore && maxHeight < settings.seaLevelOffset - 0.03f;
        const bool forceBase = node.depth < effectiveMinDepth;
        const bool mountainTerrain = maxHeight > settings.seaLevelOffset + 0.130f
                                   || featureMask > 0.40f
                                   || meshDensity > 0.42f
                                   || geometricError > 0.40f;
        const bool adaptiveSplit = !deepOcean && splitScore > (0.82f + depthBias - mountainBoost * 0.16f);
        const bool mountainSplit = mountainTerrain && node.depth < effectiveMaxDepth && splitScore > (0.64f + depthBias * 0.70f);
        if (node.depth < effectiveMaxDepth && (forceBase || adaptiveSplit || mountainSplit)) {
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
            reportChunkProgress();
            continue;
        }

        emitChunk(node, meshDensity, geometricError, featureMask, minHeight, maxHeight, hasWater, hasShore);
        reportChunkProgress();
    }

    while (advanceProgress && chunkProgressTicks < kFinalizeChunkProgressSteps) {
        ++chunkProgressTicks;
        advanceProgress("Building offline terrain chunks");
    }
}
float PlanetProceduralData::terrainHeight(const PlanetRenderSettings& settings, const glm::vec3& sphereDir)
{
    return PlanetTerrainGenerator::terrainHeight(settings, sphereDir);
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
    // 基于高度与海平面关系推导基础样本：水深、岸线掩码、温度和湿度。
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
    // 温度模型：纬度主项 + 海拔降温 + 低频噪声扰动。
    const float latitude01 = std::abs(sphereDir.y);
    const float latitudeTemperature = 1.0f - latitude01;
    const float heightCooling = std::max(height - settings.seaLevelOffset, 0.0f) * 0.35f;
    const float temperatureNoise = fbm(sphereDir * 3.0f + glm::vec3(8.1f, 2.7f, 5.4f), 4, 2.0f, 0.5f) * 0.12f;
    return glm::clamp(latitudeTemperature - heightCooling + temperatureNoise, 0.0f, 1.0f);
}

float PlanetProceduralData::moisture(const glm::vec3& sphereDir, float shoreMask)
{
    // 湿度模型：噪声底图 + 海岸增湿 + 轻度纬向修正。
    const float moistureNoise = fbm(sphereDir * 4.0f + glm::vec3(1.2f, 9.3f, 4.8f), 5, 2.0f, 0.5f) * 0.5f + 0.5f;
    const float shoreMoisture = shoreMask * 0.35f;
    const float latitudeMoisture = 1.0f - std::abs(sphereDir.y) * 0.25f;
    return glm::clamp(moistureNoise * 0.65f + shoreMoisture + latitudeMoisture * 0.15f, 0.0f, 1.0f);
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
    return gradientNoise(p);
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

void PlanetProceduralData::computeWaterClimateFields(const PlanetRenderSettings& settings,
                                                     const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Compute Water and Climate Fields");
    if (resolution_ <= 0) {
        return;
    }

    const std::size_t cellCount = static_cast<std::size_t>(resolution_ * resolution_);
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        FaceData& faceData = faces_[static_cast<std::size_t>(faceIndex)];
        for (int y = 0; y < resolution_; ++y) {
            for (int x = 0; x < resolution_; ++x) {
                const std::size_t index = static_cast<std::size_t>(y * resolution_ + x);
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution_),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution_)
                );
                const glm::vec3 sphereDir = cubeSphereDirection(kFaces[static_cast<std::size_t>(faceIndex)], uv);
                const PlanetSample sample = samplePlanetBase(settings, sphereDir, faceData.height[index]);
                faceData.waterDepth[index] = sample.waterDepth;
                faceData.shoreMask[index] = sample.shoreMask;
                faceData.temperature[index] = sample.temperature;
                faceData.moisture[index] = sample.moisture;

                const float dx = sampleFaceLayerBilinear(faceData.height, resolution_, glm::clamp(uv + glm::vec2(1.0f / resolution_, 0.0f), glm::vec2(0.0f), glm::vec2(0.999999f)))
                               - sampleFaceLayerBilinear(faceData.height, resolution_, glm::clamp(uv - glm::vec2(1.0f / resolution_, 0.0f), glm::vec2(0.0f), glm::vec2(0.999999f)));
                const float dy = sampleFaceLayerBilinear(faceData.height, resolution_, glm::clamp(uv + glm::vec2(0.0f, 1.0f / resolution_), glm::vec2(0.0f), glm::vec2(0.999999f)))
                               - sampleFaceLayerBilinear(faceData.height, resolution_, glm::clamp(uv - glm::vec2(0.0f, 1.0f / resolution_), glm::vec2(0.0f), glm::vec2(0.999999f)));
                const float slope = glm::clamp(std::sqrt(dx * dx + dy * dy) * 18.0f, 0.0f, 1.0f);
                faceData.erosionMask[index] = glm::max(faceData.erosionMask[index], slope * (1.0f - sample.waterDepth));
                faceData.featureMask[index] = glm::max(faceData.featureMask[index], sample.shoreMask * 0.25f);
            }
        }
        if (advanceProgress) {
            advanceProgress("Sampling water and climate fields");
        }
        (void)cellCount;
    }
}

void PlanetProceduralData::removeSmallTerrainSpikes(const PlanetRenderSettings& /*settings*/,
                                                    int iterations,
                                                    float threshold,
                                                    float blend)
{
    if (resolution_ <= 1) {
        return;
    }

    const int n = resolution_;
    const std::size_t count = static_cast<std::size_t>(n * n);
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (FaceData& faceData : faces_) {
            std::vector<float> next = faceData.height;
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::size_t index = static_cast<std::size_t>(y * n + x);
                    const float center = faceData.height[index];
                    float sum = 0.0f;
                    int samples = 0;
                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox == 0 && oy == 0) {
                                continue;
                            }
                            const int sx = glm::clamp(x + ox, 0, n - 1);
                            const int sy = glm::clamp(y + oy, 0, n - 1);
                            sum += faceData.height[static_cast<std::size_t>(sy * n + sx)];
                            ++samples;
                        }
                    }
                    const float average = sum / static_cast<float>(samples);
                    const float delta = average - center;
                    if (std::abs(delta) > threshold) {
                        next[index] = glm::mix(center, average, blend);
                    }
                }
            }
            faceData.height = std::move(next);
        }
        (void)count;
    }
}

void PlanetProceduralData::smoothSmallTerrainBumps(const PlanetRenderSettings& /*settings*/,
                                                   int iterations,
                                                   float blend)
{
    if (resolution_ <= 1) {
        return;
    }

    const int n = resolution_;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (FaceData& faceData : faces_) {
            std::vector<float> next = faceData.height;
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::size_t index = static_cast<std::size_t>(y * n + x);
                    float sum = faceData.height[index];
                    int samples = 1;
                    if (x > 0) {
                        sum += faceData.height[static_cast<std::size_t>(y * n + x - 1)];
                        ++samples;
                    }
                    if (x + 1 < n) {
                        sum += faceData.height[static_cast<std::size_t>(y * n + x + 1)];
                        ++samples;
                    }
                    if (y > 0) {
                        sum += faceData.height[static_cast<std::size_t>((y - 1) * n + x)];
                        ++samples;
                    }
                    if (y + 1 < n) {
                        sum += faceData.height[static_cast<std::size_t>((y + 1) * n + x)];
                        ++samples;
                    }
                    const float average = sum / static_cast<float>(samples);
                    next[index] = glm::mix(faceData.height[index], average, blend);
                }
            }
            faceData.height = std::move(next);
        }
    }
}

void PlanetProceduralData::relaxExtremeTerrainSlopes(const PlanetRenderSettings& /*settings*/,
                                                     int iterations,
                                                     float maxStep,
                                                     float blend)
{
    if (resolution_ <= 1) {
        return;
    }

    const int n = resolution_;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        for (FaceData& faceData : faces_) {
            std::vector<float> next = faceData.height;
            for (int y = 0; y < n; ++y) {
                for (int x = 0; x < n; ++x) {
                    const std::size_t index = static_cast<std::size_t>(y * n + x);
                    const float center = faceData.height[index];
                    float localMaxStep = 0.0f;
                    float localAverage = center;
                    int samples = 1;
                    const auto accumulate = [&](int sx, int sy) {
                        const float neighbor = faceData.height[static_cast<std::size_t>(sy * n + sx)];
                        localAverage += neighbor;
                        ++samples;
                        localMaxStep = std::max(localMaxStep, std::abs(neighbor - center));
                    };
                    if (x > 0) accumulate(x - 1, y);
                    if (x + 1 < n) accumulate(x + 1, y);
                    if (y > 0) accumulate(x, y - 1);
                    if (y + 1 < n) accumulate(x, y + 1);
                    localAverage /= static_cast<float>(samples);
                    if (localMaxStep > maxStep) {
                        next[index] = glm::mix(center, localAverage, blend);
                    }
                }
            }
            faceData.height = std::move(next);
        }
    }
}

void PlanetProceduralData::refineTerrainFromBiomeWeights(const PlanetRenderSettings& /*settings*/,
                                                         const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Refine Terrain From Biomes");
    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FaceData& faceData = faces_[faceIndex];
        for (std::size_t i = 0; i < faceData.height.size(); ++i) {
            const glm::vec4 a = faceData.biomeWeightA[i];
            const glm::vec4 b = faceData.biomeWeightB[i];
            const float landBias = glm::clamp(a.y * 0.35f + a.z * 0.28f + b.x * 0.52f + b.y * 0.30f, 0.0f, 1.0f);
            const float depressionBias = glm::clamp(a.w * 0.20f + b.z * 0.26f + b.w * 0.38f, 0.0f, 1.0f);
            faceData.height[i] += landBias * 0.004f - depressionBias * 0.003f;
        }
        if (advanceProgress) {
            advanceProgress("Refining terrain from biome weights");
        }
    }
}

void PlanetProceduralData::applyErosion(const PlanetRenderSettings& /*settings*/,
                                        const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Apply Erosion");
    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FaceData& faceData = faces_[faceIndex];
        for (std::size_t i = 0; i < faceData.height.size(); ++i) {
            const float slope = glm::clamp(faceData.erosionMask[i], 0.0f, 1.0f);
            const float wear = slope * 0.18f;
            faceData.wearMask[i] = glm::max(faceData.wearMask[i], wear);
            faceData.depositionMask[i] = glm::max(faceData.depositionMask[i], glm::smoothstep(0.24f, 0.72f, 1.0f - slope) * 0.25f);
            faceData.height[i] -= wear * 0.006f;
        }
        if (advanceProgress) {
            advanceProgress("Applying erosion");
        }
    }
}

void PlanetProceduralData::extractPrimaryRiver(const PlanetRenderSettings& /*settings*/,
                                               const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Extract Primary River");
    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FaceData& faceData = faces_[faceIndex];
        for (std::size_t i = 0; i < faceData.height.size(); ++i) {
            const float channel = glm::clamp(faceData.moisture[i] * 0.44f + faceData.erosionMask[i] * 0.56f, 0.0f, 1.0f);
            faceData.channelMask[i] = glm::max(faceData.channelMask[i], channel);
            faceData.flowMask[i] = glm::max(faceData.flowMask[i], glm::smoothstep(0.18f, 0.82f, channel));
        }
        if (advanceProgress) {
            advanceProgress("Extracting river paths");
        }
    }
}

void PlanetProceduralData::updateHydrologyMoisture(const PlanetRenderSettings& /*settings*/)
{
    for (FaceData& faceData : faces_) {
        for (std::size_t i = 0; i < faceData.moisture.size(); ++i) {
            faceData.moisture[i] = glm::clamp(
                faceData.moisture[i] + faceData.channelMask[i] * 0.18f + faceData.shoreMask[i] * 0.10f,
                0.0f,
                1.0f
            );
        }
    }
}

void PlanetProceduralData::computeBiomeWeights(const PlanetRenderSettings& settings,
                                               const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Compute Biome Weights");
    for (std::size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FaceData& faceData = faces_[faceIndex];
        for (int y = 0; y < resolution_; ++y) {
            for (int x = 0; x < resolution_; ++x) {
                const std::size_t index = static_cast<std::size_t>(y * resolution_ + x);
                const glm::vec2 uv(
                    (static_cast<float>(x) + 0.5f) / static_cast<float>(resolution_),
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(resolution_)
                );
                const glm::vec3 sphereDir = cubeSphereDirection(kFaces[faceIndex], uv);
                const PlanetSample sample = samplePlanetBase(settings, sphereDir, faceData.height[index]);
                const float dx = x > 0 && x + 1 < resolution_
                    ? faceData.height[static_cast<std::size_t>(y * resolution_ + (x + 1))]
                    - faceData.height[static_cast<std::size_t>(y * resolution_ + (x - 1))]
                    : 0.0f;
                const float dy = y > 0 && y + 1 < resolution_
                    ? faceData.height[static_cast<std::size_t>((y + 1) * resolution_ + x)]
                    - faceData.height[static_cast<std::size_t>((y - 1) * resolution_ + x)]
                    : 0.0f;
                const float slope = glm::clamp(std::sqrt(dx * dx + dy * dy) * 10.0f, 0.0f, 1.0f);
                const float channel = faceData.channelMask[index];
                const float flow = faceData.flowMask[index];
                const float wear = faceData.wearMask[index];
                const float deposition = faceData.depositionMask[index];
                const BiomeWeights biome = computeBiome(
                    faceData.height[index],
                    faceData.waterDepth[index],
                    faceData.shoreMask[index],
                    faceData.waterDepth[index],
                    coastalShelter(sphereDir),
                    sphereDir,
                    settings.seaLevelOffset,
                    sample.temperature,
                    sample.moisture,
                    slope,
                    channel,
                    flow,
                    wear,
                    deposition
                );
                faceData.biomeWeightA[index] = glm::vec4(biome.beach, biome.grass, biome.forest, biome.desert);
                faceData.biomeWeightB[index] = glm::vec4(biome.rock, biome.snow, biome.wetland, biome.shallowWater);
                faceData.domainWeight[index] = glm::vec4(sample.temperature, sample.moisture, slope, glm::clamp(channel + flow, 0.0f, 1.0f));
            }
        }
        if (advanceProgress) {
            advanceProgress("Computing biome weights");
        }
    }
}

void PlanetProceduralData::computeMeshPlanningFields(const PlanetRenderSettings& /*settings*/,
                                                     const std::function<void(const char*)>& advanceProgress)
{
    PROFILE_SCOPE("Compute Mesh Planning Fields");
    for (FaceData& faceData : faces_) {
        for (std::size_t i = 0; i < faceData.height.size(); ++i) {
            const float slope = glm::clamp(faceData.erosionMask[i] * 0.75f + faceData.flowMask[i] * 0.35f, 0.0f, 1.0f);
            const float feature = glm::clamp(faceData.featureMask[i] + faceData.channelMask[i] * 0.22f + faceData.shoreMask[i] * 0.12f, 0.0f, 1.0f);
            faceData.meshDensity[i] = glm::clamp(0.22f + slope * 0.58f + feature * 0.40f, 0.0f, 1.0f);
            faceData.geometricError[i] = glm::clamp(0.15f + slope * 0.52f + feature * 0.28f, 0.0f, 1.0f);
        }
        if (advanceProgress) {
            advanceProgress("Computing mesh planning fields");
        }
    }
}

void PlanetProceduralData::smoothBiomeWeights(int radius, float blend)
{
    if (radius <= 0 || resolution_ <= 1) {
        return;
    }

    const int n = resolution_;
    const int r = std::max(radius, 1);
    for (FaceData& faceData : faces_) {
        std::vector<glm::vec4> nextA = faceData.biomeWeightA;
        std::vector<glm::vec4> nextB = faceData.biomeWeightB;
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                glm::vec4 sumA(0.0f);
                glm::vec4 sumB(0.0f);
                int samples = 0;
                for (int oy = -r; oy <= r; ++oy) {
                    for (int ox = -r; ox <= r; ++ox) {
                        const int sx = glm::clamp(x + ox, 0, n - 1);
                        const int sy = glm::clamp(y + oy, 0, n - 1);
                        const std::size_t index = static_cast<std::size_t>(sy * n + sx);
                        sumA += faceData.biomeWeightA[index];
                        sumB += faceData.biomeWeightB[index];
                        ++samples;
                    }
                }
                const std::size_t index = static_cast<std::size_t>(y * n + x);
                const glm::vec4 avgA = sumA / static_cast<float>(samples);
                const glm::vec4 avgB = sumB / static_cast<float>(samples);
                nextA[index] = glm::mix(faceData.biomeWeightA[index], avgA, blend);
                nextB[index] = glm::mix(faceData.biomeWeightB[index], avgB, blend);
            }
        }
        faceData.biomeWeightA = std::move(nextA);
        faceData.biomeWeightB = std::move(nextB);
    }
}
