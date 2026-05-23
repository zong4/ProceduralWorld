#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

#include <glm/glm.hpp>

#include "PlanetHeightField.h"
#include "PlanetRenderer.h"

// CPU 端程序化星球数据生成器。
// 它把整颗星球烘焙成 6 张 cube-face 高度/水文/气候/biome 数据图，
// renderer 再把这些数据上传为 sampler2DArray 供 shader 采样。
class PlanetProceduralData
{
public:
    // 生成流程拆成多个模块，UI 用它显示进度条和当前阶段说明。
    enum class GenerationModule : int {
        BaseTerrain = 0,
        InitialClimate = 1,
        InitialBiomes = 2,
        BiomeTerrain = 3,
        Erosion = 4,
        FinalClimate = 5,
        FinalBiomes = 6,
        Finalize = 7,
        Count = 8
    };

    // 后台生成线程向主线程报告的进度快照。
    struct GenerationProgress {
        int completedSteps = 0;
        int totalSteps = 1;
        GenerationModule module = GenerationModule::BaseTerrain;
        int moduleCompletedSteps = 0;
        int moduleTotalSteps = 1;
        const char* status = "";
    };

    using ProgressCallback = std::function<void(const GenerationProgress& progress)>;

    // 单个 cube face 的所有烘焙层。
    // height 是归一化高度；waterDepth 已乘过 terrainHeightScale，单位接近世界空间；
    // biomeWeightA/B 用两个 vec4 打包 8 类材质权重。
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

    // 只读访问器，渲染器和 UI 通过这些函数取得生成结果和统计信息。
    bool isGenerated() const { return generated_; }
    int resolution() const { return resolution_; }
    const PlanetRenderSettings& settings() const { return settings_; }
    const std::array<FaceData, 6>& faces() const { return faces_; }
    PlanetGlobalHeightField globalHeightField() const;

    float minHeight() const { return minHeight_; }
    float maxHeight() const { return maxHeight_; }
    float maxWaterDepth() const { return maxWaterDepth_; }
    float waterCoverage() const { return waterCoverage_; }
    float shoreCoverage() const { return shoreCoverage_; }

private:
    // cube face 局部坐标系：normal 为面朝向，axisU/axisV 为该面 UV 两个轴。
    struct FaceBasis {
        glm::vec3 normal;
        glm::vec3 axisU;
        glm::vec3 axisV;
    };

    // 跨 cube face 查询邻居时使用的“面 + 索引”引用。
    struct CellRef {
        int face = 0;
        std::size_t index = 0;
    };

    // 单点程序化采样的中间结果。
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
    // 把球面方向映射回某个 cube face 的离散格子。
    static CellRef cellFromDirection(const glm::vec3& dir, int resolution);
    // 支持越过 face 边界的邻居查询，是接缝修复和侵蚀的基础。
    static CellRef neighborCell(int faceIndex, int x, int y, int resolution);
    static glm::vec3 hash3(const glm::vec3& p);
    static float gradientNoise(const glm::vec3& p);
    // fractal Brownian motion：多层 gradient noise 叠加。
    static float fbm(const glm::vec3& p, int octaves, float lacunarity, float gain);

    static float altitudeBandWeight(float startAltitude, float endAltitude);
    // 基础地形高度函数：大陆、海盆、高地、山脉、峰顶和海沟都在这里合成。
    static float terrainHeight(const PlanetRenderSettings& settings, const glm::vec3& sphereDir);
    static PlanetSample samplePlanetBase(const PlanetRenderSettings& settings, const glm::vec3& sphereDir);
    static PlanetSample samplePlanetBase(const PlanetRenderSettings& settings, const glm::vec3& sphereDir, float height);
    void computeWaterClimateFields(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void refineTerrainFromBiomeWeights(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void applyErosion(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void extractPrimaryRiver(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void updateHydrologyMoisture(const PlanetRenderSettings& settings);
    void computeBiomeWeights(const PlanetRenderSettings& settings, const std::function<void(const char*)>& advanceProgress);
    void smoothBiomeWeights(int radius, float blend);
    void fixCubeFaceSeams();
    static float temperature(const PlanetRenderSettings& settings, const glm::vec3& sphereDir, float height);
    static float moisture(const glm::vec3& sphereDir, float shoreMask);
};
