#include "PlanetProceduralData.h"

#include <algorithm>
#include <cmath>
#include <limits>

const std::array<PlanetProceduralData::FaceBasis, 6> PlanetProceduralData::kFaces = {{
    {{ 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f, -1.0f}, { 0.0f,  1.0f,  0.0f}},
    {{-1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f,  1.0f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.0f,  1.0f,  0.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f, -1.0f}},
    {{ 0.0f, -1.0f,  0.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.0f,  0.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.0f,  0.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}}
}};

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
        faceData.temperature.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.moisture.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);
        faceData.biomeId.assign(static_cast<std::size_t>(resolution_ * resolution_), 0.0f);

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

    std::size_t totalSamples = 0;
    std::size_t waterSamples = 0;
    std::size_t shoreSamples = 0;

    const float seaLevelRadius = settings.planetRadius + settings.seaLevelOffset * settings.terrainHeightScale;
    const float shoreWidth = std::max(settings.oceanShoreBlendWidth, 0.001f);

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
                const float normalizedHeight = faceData.height[index];
                const float terrainRadius = settings.planetRadius + normalizedHeight * settings.terrainHeightScale;
                const float signedWaterDepth = seaLevelRadius - terrainRadius;
                const float waterDepth = std::max(signedWaterDepth, 0.0f);
                const float shoreMask = 1.0f - glm::smoothstep(0.0f, shoreWidth, std::abs(signedWaterDepth));
                const float temperatureValue = temperature(settings, sphereDir, normalizedHeight);
                const float moistureValue = moisture(sphereDir, shoreMask);
                const float biomeValue = classifyBiome(normalizedHeight, waterDepth, shoreMask, temperatureValue, moistureValue);

                faceData.waterDepth[index] = waterDepth;
                faceData.shoreMask[index] = shoreMask;
                faceData.temperature[index] = temperatureValue;
                faceData.moisture[index] = moistureValue;
                faceData.biomeId[index] = biomeValue;

                minHeight_ = std::min(minHeight_, normalizedHeight);
                maxHeight_ = std::max(maxHeight_, normalizedHeight);
                maxWaterDepth_ = std::max(maxWaterDepth_, waterDepth);
                waterSamples += waterDepth > 0.0f ? 1 : 0;
                shoreSamples += shoreMask > 0.05f ? 1 : 0;
                ++totalSamples;
            }
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

void PlanetProceduralData::applyErosion(const PlanetRenderSettings& settings)
{
    const int iterations = std::clamp(settings.erosionIterations, 0, 256);
    const float erosionStrength = std::max(settings.erosionStrength, 0.0f);
    const float thermalStrength = std::max(settings.erosionThermalStrength, 0.0f);
    if (iterations <= 0 || (erosionStrength <= 0.0f && thermalStrength <= 0.0f)) {
        return;
    }

    const int n = resolution_;
    const float talus = std::max(settings.erosionTalus, 0.0001f);
    const float sediment = glm::clamp(settings.erosionSediment, 0.0f, 1.0f);
    const float seaLevel = settings.seaLevelOffset;
    const auto indexOf = [n](int x, int y) {
        return static_cast<std::size_t>(y * n + x);
    };

    for (FaceData& faceData : faces_) {
        faceData.erosionMask.assign(faceData.height.size(), 0.0f);
        std::vector<float> delta(faceData.height.size(), 0.0f);

        for (int iteration = 0; iteration < iterations; ++iteration) {
            std::fill(delta.begin(), delta.end(), 0.0f);

            for (int y = 1; y < n - 1; ++y) {
                for (int x = 1; x < n - 1; ++x) {
                    const std::size_t center = indexOf(x, y);
                    const float h = faceData.height[center];
                    const float landMask = glm::smoothstep(seaLevel + 0.02f, seaLevel + 0.20f, h);
                    if (landMask <= 0.001f) {
                        continue;
                    }

                    float lowestHeight = h;
                    std::size_t lowestIndex = center;
                    for (int oy = -1; oy <= 1; ++oy) {
                        for (int ox = -1; ox <= 1; ++ox) {
                            if (ox == 0 && oy == 0) {
                                continue;
                            }
                            const std::size_t neighbor = indexOf(x + ox, y + oy);
                            const float neighborHeight = faceData.height[neighbor];
                            if (neighborHeight < lowestHeight) {
                                lowestHeight = neighborHeight;
                                lowestIndex = neighbor;
                            }
                        }
                    }

                    const float slope = h - lowestHeight;
                    if (lowestIndex != center && slope > talus) {
                        const float channel = std::min((slope - talus) * erosionStrength * landMask, slope * 0.38f);
                        delta[center] -= channel;
                        delta[lowestIndex] += channel * sediment;
                    }

                    if (thermalStrength > 0.0f) {
                        for (int oy = -1; oy <= 1; ++oy) {
                            for (int ox = -1; ox <= 1; ++ox) {
                                if ((ox == 0 && oy == 0) || (ox != 0 && oy != 0)) {
                                    continue;
                                }
                                const std::size_t neighbor = indexOf(x + ox, y + oy);
                                const float slopeToNeighbor = h - faceData.height[neighbor];
                                if (slopeToNeighbor > talus * 1.45f) {
                                    const float slide = std::min(
                                        (slopeToNeighbor - talus * 1.45f) * thermalStrength * landMask,
                                        slopeToNeighbor * 0.18f
                                    );
                                    delta[center] -= slide;
                                    delta[neighbor] += slide;
                                }
                            }
                        }
                    }
                }
            }

            for (std::size_t i = 0; i < faceData.height.size(); ++i) {
                faceData.height[i] += delta[i];
                faceData.erosionMask[i] += std::abs(delta[i]);
            }
        }

        float maxErosion = 0.0f;
        for (float value : faceData.erosionMask) {
            maxErosion = std::max(maxErosion, value);
        }
        if (maxErosion > 0.000001f) {
            for (float& value : faceData.erosionMask) {
                value = std::sqrt(glm::clamp(value / maxErosion, 0.0f, 1.0f));
            }
        }
    }
}

glm::vec3 PlanetProceduralData::cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv)
{
    const glm::vec2 faceUv = uv * 2.0f - 1.0f;
    return glm::normalize(face.normal + faceUv.x * face.axisU + faceUv.y * face.axisV);
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
    return h;
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

float PlanetProceduralData::classifyBiome(float height, float waterDepth, float shoreMask, float temperature, float moisture)
{
    if (waterDepth > 0.08f) {
        return 0.0f;
    }
    if (waterDepth > 0.0f) {
        return 1.0f;
    }
    if (shoreMask > 0.45f) {
        return 2.0f;
    }
    if (temperature < 0.22f || height > 0.78f) {
        return 7.0f;
    }
    if (height > 0.62f) {
        return 6.0f;
    }
    if (temperature > 0.62f && moisture < 0.32f) {
        return 5.0f;
    }
    if (moisture > 0.58f && temperature > 0.32f) {
        return 4.0f;
    }
    return 3.0f;
}
