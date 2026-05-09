#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

#include <glm/glm.hpp>

#include "PlanetRenderer.h"

class PlanetProceduralData
{
public:
    enum class GenerationModule : int {
        TerrainHeight = 0,
        Erosion = 1,
        Climate = 2,
        Biome = 3,
        Finalize = 4,
        Count = 5
    };

    struct GenerationProgress {
        int completedSteps = 0;
        int totalSteps = 1;
        GenerationModule module = GenerationModule::TerrainHeight;
        int moduleCompletedSteps = 0;
        int moduleTotalSteps = 1;
        const char* status = "";
    };

    using ProgressCallback = std::function<void(const GenerationProgress& progress)>;

    struct FaceData {
        int resolution = 0;
        std::vector<float> height;
        std::vector<float> waterDepth;
        std::vector<float> shoreMask;
        std::vector<float> erosionMask;
        std::vector<float> channelMask;
        std::vector<float> flowMask;
        std::vector<float> wearMask;
        std::vector<float> depositionMask;
        std::vector<float> temperature;
        std::vector<float> moisture;
        std::vector<glm::vec4> biomeWeightA;
        std::vector<glm::vec4> biomeWeightB;
    };

    void generate(const PlanetRenderSettings& settings, int faceResolution);
    void generate(const PlanetRenderSettings& settings, int faceResolution, const ProgressCallback& progressCallback);
    bool saveCache(const char* path) const;
    bool loadCache(const char* path, const PlanetRenderSettings& settings);
    void clear();

    bool isGenerated() const { return generated_; }
    int resolution() const { return resolution_; }
    const PlanetRenderSettings& settings() const { return settings_; }
    const std::array<FaceData, 6>& faces() const { return faces_; }

    float minHeight() const { return minHeight_; }
    float maxHeight() const { return maxHeight_; }
    float maxWaterDepth() const { return maxWaterDepth_; }
    float waterCoverage() const { return waterCoverage_; }
    float shoreCoverage() const { return shoreCoverage_; }

private:
    struct FaceBasis {
        glm::vec3 normal;
        glm::vec3 axisU;
        glm::vec3 axisV;
    };

    struct CellRef {
        int face = 0;
        std::size_t index = 0;
    };

    struct PlanetSample {
        float height = 0.0f;
        float waterDepth = 0.0f;
        float shoreMask = 0.0f;
        float temperature = 0.0f;
        float moisture = 0.0f;
        float slope = 0.0f;
        float channel = 0.0f;
        float flow = 0.0f;
        float wear = 0.0f;
        float deposition = 0.0f;
        glm::vec4 biomeA = glm::vec4(0.0f);
        glm::vec4 biomeB = glm::vec4(0.0f);
    };

    static const std::array<FaceBasis, 6> kFaces;

    bool generated_ = false;
    int resolution_ = 0;
    PlanetRenderSettings settings_{};
    std::array<FaceData, 6> faces_{};
    float minHeight_ = 0.0f;
    float maxHeight_ = 0.0f;
    float maxWaterDepth_ = 0.0f;
    float waterCoverage_ = 0.0f;
    float shoreCoverage_ = 0.0f;

    static glm::vec3 cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv);
    static int faceIndexFromDirection(const glm::vec3& dir);
    static CellRef cellFromDirection(const glm::vec3& dir, int resolution);
    static CellRef neighborCell(int faceIndex, int x, int y, int resolution);
    static glm::vec3 hash3(const glm::vec3& p);
    static float gradientNoise(const glm::vec3& p);
    static float fbm(const glm::vec3& p, int octaves, float lacunarity, float gain);

    static float altitudeBandWeight(float startAltitude, float endAltitude);
    static float terrainHeight(const PlanetRenderSettings& settings, const glm::vec3& sphereDir);
    static PlanetSample samplePlanetBase(const PlanetRenderSettings& settings, const glm::vec3& sphereDir);
    static PlanetSample samplePlanetBase(const PlanetRenderSettings& settings, const glm::vec3& sphereDir, float height);
    void applyErosion(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void computeBiomeWeights(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void fixCubeFaceSeams();
    static float temperature(const PlanetRenderSettings& settings, const glm::vec3& sphereDir, float height);
    static float moisture(const glm::vec3& sphereDir, float shoreMask);
};
