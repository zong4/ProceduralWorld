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
constexpr char kProceduralCacheMagic[8] = { 'P', 'W', 'C', 'A', 'C', 'H', 'E', '9' };
constexpr std::uint32_t kProceduralCacheVersion = 9;

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

BiomeWeights computeBiome(float height,
                          float waterDepth,
                          float shore,
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

    biome.shallowWater = glm::smoothstep(0.001f, 0.040f, waterDepth)
                       * (1.0f - glm::smoothstep(0.050f, 0.180f, waterDepth));

    biome.beach = landMask
                * shore
                * (1.0f - glm::smoothstep(0.20f, 0.48f, slope))
                * (1.0f - glm::smoothstep(0.45f, 0.75f, wear));

    biome.wetland = landMask
                  * (shore * 0.35f + channel * 0.20f + flow * 0.45f + deposition * 0.35f)
                  * moisture
                  * (1.0f - glm::smoothstep(0.25f, 0.55f, slope));

    biome.rock = landMask * (
        glm::smoothstep(0.42f, 0.72f, slope) * 0.75f
      + wear * 0.65f
      + channel * 0.12f
      + glm::smoothstep(0.62f, 0.82f, height) * 0.35f
    );

    biome.snow = landMask * (
        glm::smoothstep(0.62f, 0.86f, cold)
      + glm::smoothstep(0.75f, 0.92f, height) * 0.65f
    );

    biome.desert = landMask
                 * glm::smoothstep(0.55f, 0.78f, temperature)
                 * glm::smoothstep(0.45f, 0.75f, dry)
                 * (1.0f - shore)
                 * (1.0f - glm::smoothstep(0.30f, 0.62f, slope));

    biome.forest = landMask
                 * glm::smoothstep(0.48f, 0.78f, moisture)
                 * glm::smoothstep(0.25f, 0.55f, temperature)
                 * (1.0f - glm::smoothstep(0.35f, 0.65f, slope));

    biome.grass = landMask
                * glm::smoothstep(0.18f, 0.55f, moisture)
                * glm::smoothstep(0.20f, 0.70f, temperature)
                * (1.0f - biome.desert * 0.75f)
                * (1.0f - biome.rock * 0.55f)
                * (1.0f - biome.snow * 0.85f);

    biome.snow = glm::clamp(biome.snow, 0.0f, 1.0f);
    biome.rock = glm::clamp(biome.rock * (1.0f - biome.snow * 0.45f), 0.0f, 1.0f);
    biome.forest *= (1.0f - biome.desert * 0.85f);
    biome.forest *= (1.0f - biome.snow * 0.95f);
    biome.forest *= (1.0f - biome.rock * 0.65f);
    biome.forest *= (1.0f - biome.beach * 0.90f);
    biome.grass *= (1.0f - biome.beach * 0.75f);
    biome.grass *= (1.0f - biome.snow * 0.80f);
    biome.grass *= (1.0f - biome.rock * 0.55f);
    biome.desert *= (1.0f - biome.wetland * 0.80f);
    biome.beach *= (1.0f - biome.rock * 0.45f);

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
    settings_ = settings;
    resolution_ = std::clamp(faceResolution, 16, 512);
    generated_ = false;

    minHeight_ = std::numeric_limits<float>::max();
    maxHeight_ = std::numeric_limits<float>::lowest();
    maxWaterDepth_ = 0.0f;

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
        }
    }

    applyErosion(settings);
    fixCubeFaceSeams();

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
        }
    }

    fixCubeFaceSeams();
    computeBiomeWeights(settings);
    fixCubeFaceSeams();

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
    }

    if (totalSamples > 0) {
        waterCoverage_ = static_cast<float>(waterSamples) / static_cast<float>(totalSamples);
        shoreCoverage_ = static_cast<float>(shoreSamples) / static_cast<float>(totalSamples);
    } else {
        waterCoverage_ = 0.0f;
        shoreCoverage_ = 0.0f;
    }

    generated_ = true;
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

void PlanetProceduralData::computeBiomeWeights(const PlanetRenderSettings& settings)
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

                const BiomeWeights biome = computeBiome(
                    sample.height,
                    sample.waterDepth,
                    sample.shoreMask,
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
        }
    }
}

void PlanetProceduralData::applyErosion(const PlanetRenderSettings& settings)
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
                float channel = flow * concavityGate * slopeGate;
                channel += wear * concavityGate * 0.18f;
                channelRaw[static_cast<std::size_t>(faceIndex)][center] = std::pow(glm::clamp(channel, 0.0f, 1.0f), 1.8f);
            }
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

float PlanetProceduralData::altitudeBandWeight(float startAltitude, float endAltitude)
{
    return 1.0f - glm::smoothstep(startAltitude, endAltitude, 0.0f);
}

float PlanetProceduralData::terrainHeight(const PlanetRenderSettings& settings, const glm::vec3& sphereDir)
{
    const glm::vec3 p = sphereDir * settings.terrainNoiseScale;
    const glm::vec3 warp(
        fbm(p + glm::vec3(3.1f, 0.0f, 0.0f), 4, 2.0f, 0.5f),
        fbm(p + glm::vec3(0.0f, 4.7f, 0.0f), 4, 2.0f, 0.5f),
        fbm(p + glm::vec3(0.0f, 0.0f, 5.3f), 4, 2.0f, 0.5f)
    );

    const float continents = fbm(p + 1.8f * warp, 5, 2.0f, 0.5f);
    const float continentalShelf = glm::smoothstep(-0.30f, 0.38f, continents);

    const glm::vec3 mountainP = p * settings.mountainMaskScale + warp * 1.35f;
    float mountainBands = 1.0f - std::abs(gradientNoise(mountainP));
    mountainBands = std::pow(glm::clamp(mountainBands, 0.0f, 1.0f), 1.85f);
    const float mountainField = fbm(mountainP * 0.72f + glm::vec3(11.7f, 2.3f, 6.1f), 4, 2.05f, 0.50f) * 0.5f + 0.5f;
    float mountainMask = glm::smoothstep(0.44f, 0.78f, mountainBands * 0.68f + mountainField * 0.32f);
    mountainMask *= continentalShelf;
    mountainMask = std::pow(glm::clamp(mountainMask, 0.0f, 1.0f), 1.15f);

    float massifBase = fbm(mountainP * 0.35f + glm::vec3(14.3f, 5.2f, 8.7f), 4, 2.0f, 0.5f) * 0.5f + 0.5f;
    massifBase = glm::smoothstep(0.46f, 0.72f, massifBase) * mountainMask;
    const float massifShape = fbm(mountainP * 0.95f + glm::vec3(6.4f, 17.8f, 3.1f), 4, 2.0f, 0.5f);

    float h = continents * 0.72f;
    h += settings.mountainMaskStrength * massifBase * (0.20f + massifShape * 0.08f);
    h += settings.mountainMaskStrength * mountainMask * (0.16f + mountainField * 0.14f);
    h = (h < 0.0f ? -1.0f : 1.0f) * std::pow(std::abs(h), 1.15f);

    const float relativeToSea = h - settings.seaLevelOffset;
    const float landUpliftMask = glm::smoothstep(-0.025f, 0.18f, relativeToSea) * continentalShelf;
    if (landUpliftMask > 0.0f) {
        const float broadUpliftNoise = fbm(p * 0.82f + warp * 0.55f + glm::vec3(18.2f, 3.8f, 27.6f), 4, 2.0f, 0.52f) * 0.5f + 0.5f;
        const float plateauNoise = fbm(p * 1.65f + warp * 0.75f + glm::vec3(7.4f, 22.1f, 4.3f), 4, 2.05f, 0.50f) * 0.5f + 0.5f;
        const float lowlandMask = glm::smoothstep(-0.02f, 0.12f, relativeToSea) * (1.0f - glm::smoothstep(0.18f, 0.38f, relativeToSea));
        const float plateauMask = glm::smoothstep(0.12f, 0.38f, relativeToSea) * glm::smoothstep(0.44f, 0.72f, plateauNoise);
        const float continentalInterior = glm::smoothstep(0.18f, 0.70f, continentalShelf);
        const float uplift =
            0.035f * lowlandMask +
            0.060f * broadUpliftNoise * continentalInterior +
            0.105f * plateauMask +
            0.045f * mountainMask;
        h += uplift * landUpliftMask;
    }

    const float signedWaterDepth = settings.seaLevelOffset - h;
    const float oceanMask = glm::smoothstep(0.0f, 0.045f, signedWaterDepth);
    if (oceanMask > 0.0f) {
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
    const float latitude01 = std::abs(sphereDir.y);
    const float latitudeTemperature = 1.0f - latitude01;
    const float heightCooling = std::max(height - settings.seaLevelOffset, 0.0f) * 0.35f;
    const float temperatureNoise = fbm(sphereDir * 3.0f + glm::vec3(8.1f, 2.7f, 5.4f), 4, 2.0f, 0.5f) * 0.12f;
    return glm::clamp(latitudeTemperature - heightCooling + temperatureNoise, 0.0f, 1.0f);
}

float PlanetProceduralData::moisture(const glm::vec3& sphereDir, float shoreMask)
{
    const float moistureNoise = fbm(sphereDir * 4.0f + glm::vec3(1.2f, 9.3f, 4.8f), 5, 2.0f, 0.5f) * 0.5f + 0.5f;
    const float shoreMoisture = shoreMask * 0.35f;
    const float latitudeMoisture = 1.0f - std::abs(sphereDir.y) * 0.25f;
    return glm::clamp(moistureNoise * 0.65f + shoreMoisture + latitudeMoisture * 0.15f, 0.0f, 1.0f);
}
