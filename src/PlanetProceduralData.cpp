#include "PlanetProceduralData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>

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
constexpr std::uint32_t kProceduralCacheVersion = 33;

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
            || !writeVec4Array(file, faceData.biomeWeightA)
            || !writeVec4Array(file, faceData.biomeWeightB)) {
            return false;
        }
    }

    return static_cast<bool>(file);
}

bool PlanetProceduralData::loadCache(const char* path, const PlanetRenderSettings& settings)
{
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
            || !readVec4Array(file, faceData.biomeWeightA, expectedCount)
            || !readVec4Array(file, faceData.biomeWeightB, expectedCount)) {
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
        target.vectorLayers[vectorIndex(PlanetVectorLayer::BiomeWeightA)] = source.biomeWeightA;
        target.vectorLayers[vectorIndex(PlanetVectorLayer::BiomeWeightB)] = source.biomeWeightB;
    }

    return heightField;
}

void PlanetProceduralData::clear()
{
    generated_ = false;
    resolution_ = 0;
    faces_ = {};
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
    moduleTotals[static_cast<std::size_t>(GenerationModule::BaseTerrain)] = resolution_ * 6;
    moduleTotals[static_cast<std::size_t>(GenerationModule::InitialClimate)] = resolution_ * 6 + 1;
    moduleTotals[static_cast<std::size_t>(GenerationModule::InitialBiomes)] = resolution_ * 6 + 2;
    moduleTotals[static_cast<std::size_t>(GenerationModule::BiomeTerrain)] = resolution_ * 6 + 1;
    moduleTotals[static_cast<std::size_t>(GenerationModule::Erosion)] =
        erosionActive ? erosionIterations + thermalIterations + 8 : 0;
    moduleTotals[static_cast<std::size_t>(GenerationModule::FinalClimate)] = resolution_ * 6 + 3;
    moduleTotals[static_cast<std::size_t>(GenerationModule::FinalBiomes)] = resolution_ * 6 + 2;
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
        faceData.biomeWeightA.assign(static_cast<std::size_t>(resolution_ * resolution_), glm::vec4(0.0f));
        faceData.biomeWeightB.assign(static_cast<std::size_t>(resolution_ * resolution_), glm::vec4(0.0f));

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
    fixCubeFaceSeams();
    advanceModuleProgress(GenerationModule::BiomeTerrain, "Blending biome-shaped terrain seams");

    applyErosion(settings, advanceErosionProgress);
    extractPrimaryRiver(settings, advanceErosionProgress);
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
    reconcileVec4Field(&FaceData::biomeWeightA, materialSeamRings);
    reconcileVec4Field(&FaceData::biomeWeightB, materialSeamRings);
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

                const float dune = fbm(sphereDir * 19.0f + glm::vec3(23.1f, 7.4f, 11.8f), 4, 2.0f, 0.48f);
                const float forestRelief = fbm(sphereDir * 12.0f + glm::vec3(4.2f, 19.7f, 8.5f), 4, 2.0f, 0.50f);
                const float alpineRelief = fbm(sphereDir * 8.5f + glm::vec3(17.2f, 3.6f, 21.4f), 4, 2.05f, 0.50f);

                float height = faceData.height[index];
                const float lowPlainTarget = seaLevel + 0.055f + dune * 0.020f;
                const float desertPlain = desert * (1.0f - glm::smoothstep(0.18f, 0.46f, height - seaLevel));
                height = glm::mix(height, lowPlainTarget, desertPlain * 0.42f);
                height += desert * dune * 0.018f;
                height += grass * forestRelief * 0.006f;
                height += forest * forestRelief * 0.012f;
                height += (rock * 0.018f + snow * 0.012f) * std::max(alpineRelief, 0.0f);
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

float PlanetProceduralData::altitudeBandWeight(float startAltitude, float endAltitude)
{
    return 1.0f - glm::smoothstep(startAltitude, endAltitude, 0.0f);
}

float PlanetProceduralData::terrainHeight(const PlanetRenderSettings& settings, const glm::vec3& sphereDir)
{
    // 基础高度合成顺序：大陆轮廓 -> 高地/盆地 -> 山脉/峰顶 -> 海底地貌。
    const glm::vec3 p = sphereDir * settings.terrainNoiseScale;
    const glm::vec3 warp(
        fbm(p + glm::vec3(3.1f, 0.0f, 0.0f), 4, 2.0f, 0.5f),
        fbm(p + glm::vec3(0.0f, 4.7f, 0.0f), 4, 2.0f, 0.5f),
        fbm(p + glm::vec3(0.0f, 0.0f, 5.3f), 4, 2.0f, 0.5f)
    );

    const float continents = fbm(p + 1.8f * warp, 5, 2.0f, 0.5f);
    const float continentalShelf = glm::smoothstep(-0.30f, 0.38f, continents);
    const float landCore = glm::smoothstep(-0.04f, 0.34f, continents - settings.seaLevelOffset * 0.72f);
    const float highlandRaw = fbm(p * 0.48f + warp * 0.50f + glm::vec3(18.2f, 3.8f, 27.6f), 4, 2.0f, 0.52f) * 0.5f + 0.5f;
    float highlandShoulder = glm::smoothstep(0.44f, 0.66f, highlandRaw) * continentalShelf * landCore;
    float highlandCore = glm::smoothstep(0.60f, 0.80f, highlandRaw) * continentalShelf * landCore;
    highlandShoulder = std::pow(glm::clamp(highlandShoulder, 0.0f, 1.0f), 1.10f);
    highlandCore = std::pow(glm::clamp(highlandCore, 0.0f, 1.0f), 1.35f);
    const float highlandMask = glm::clamp(highlandShoulder * 0.65f + highlandCore, 0.0f, 1.0f);
    const float highlandVariation = fbm(p * 1.10f + warp * 0.70f + glm::vec3(7.4f, 22.1f, 4.3f), 4, 2.0f, 0.50f) * 0.5f + 0.5f;

    float basinMask = fbm(p * 0.58f + warp * 0.30f + glm::vec3(8.7f, 2.4f, 13.1f), 4, 2.0f, 0.50f) * 0.5f + 0.5f;
    basinMask = glm::smoothstep(0.62f, 0.82f, basinMask) * continentalShelf * (1.0f - highlandCore * 0.70f);
    basinMask *= glm::smoothstep(-0.08f, 0.24f, continents - settings.seaLevelOffset * 0.60f);

    const glm::vec3 mountainP = p * settings.mountainMaskScale + warp * 1.35f;
    float mountainBands = 1.0f - std::abs(gradientNoise(mountainP));
    mountainBands = std::pow(glm::clamp(mountainBands, 0.0f, 1.0f), 1.85f);
    const float mountainField = fbm(mountainP * 0.72f + glm::vec3(11.7f, 2.3f, 6.1f), 4, 2.05f, 0.50f) * 0.5f + 0.5f;
    float mountainRegionMask = fbm(sphereDir * 1.15f + warp * 0.55f + glm::vec3(29.3f, 7.8f, 18.6f), 4, 2.0f, 0.52f) * 0.5f + 0.5f;
    mountainRegionMask = glm::smoothstep(0.62f, 0.82f, mountainRegionMask) * continentalShelf;
    mountainRegionMask *= landCore;
    mountainRegionMask *= glm::mix(0.72f, 1.28f, highlandMask);
    mountainRegionMask *= 1.0f - basinMask * 0.50f;
    mountainRegionMask = std::pow(glm::clamp(mountainRegionMask, 0.0f, 1.0f), 1.20f);
    float mountainMask = glm::smoothstep(0.50f, 0.82f, mountainBands * 0.68f + mountainField * 0.32f);
    mountainMask *= mountainRegionMask;
    mountainMask = std::pow(glm::clamp(mountainMask, 0.0f, 1.0f), 1.15f);

    float massifBase = fbm(mountainP * 0.35f + glm::vec3(14.3f, 5.2f, 8.7f), 4, 2.0f, 0.5f) * 0.5f + 0.5f;
    massifBase = glm::smoothstep(0.46f, 0.72f, massifBase) * mountainMask;
    const float massifShape = fbm(mountainP * 0.95f + glm::vec3(6.4f, 17.8f, 3.1f), 4, 2.0f, 0.5f);
    const float summitNoise = fbm(mountainP * 1.85f + warp * 1.45f + glm::vec3(32.6f, 11.4f, 5.9f), 4, 2.1f, 0.48f) * 0.5f + 0.5f;
    float summitPeakMask = glm::smoothstep(0.74f, 0.91f, summitNoise)
                         * glm::smoothstep(0.58f, 0.88f, mountainMask)
                         * std::pow(glm::clamp(mountainRegionMask, 0.0f, 1.0f), 1.8f);

    float h = continents * 0.55f;
    h += highlandShoulder * (0.06f + highlandVariation * 0.04f);
    h += highlandCore * (0.16f + highlandVariation * 0.10f);
    h -= basinMask * (0.070f + (1.0f - highlandVariation) * 0.060f);
    h += settings.mountainMaskStrength * massifBase * (0.205f + massifShape * 0.058f);
    h += settings.mountainMaskStrength * mountainMask * (0.068f + mountainField * 0.056f);
    h += settings.mountainMaskStrength * summitPeakMask * (0.275f + massifShape * 0.082f);
    h = (h < 0.0f ? -1.0f : 1.0f) * std::pow(std::abs(h), 1.15f);

    const float relativeToSea = h - settings.seaLevelOffset;
    const float landUpliftMask = glm::smoothstep(-0.025f, 0.18f, relativeToSea) * continentalShelf;
    if (landUpliftMask > 0.0f) {
        const float lowlandMask = glm::smoothstep(-0.02f, 0.12f, relativeToSea) * (1.0f - glm::smoothstep(0.18f, 0.38f, relativeToSea));
        const float plateauMask = highlandMask * glm::smoothstep(0.10f, 0.34f, relativeToSea);
        const float continentalInterior = glm::smoothstep(0.18f, 0.70f, continentalShelf);
        const float uplift =
            0.030f * lowlandMask +
            0.050f * highlandShoulder * highlandVariation * continentalInterior +
            0.085f * highlandCore +
            0.030f * mountainMask;
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
        const float basinNoise = fbm(p * 1.35f + warp * 0.65f + glm::vec3(21.4f, 6.2f, 13.8f), 4, 2.0f, 0.52f) * 0.5f + 0.5f;
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
