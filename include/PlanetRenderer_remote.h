#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "FFTOcean.h"
#include "FlyCamera.h"
#include "ShaderProgram.h"

class PlanetProceduralData;

// 地形调试/展示模式�?
enum class PlanetRenderMode : int {
    Shaded = 0,
    Unshaded = 1,
    HeightMap = 2,
    Normals = 3
};

// 线框叠加模式：可选择显示陆地 patch 或海�?patch 的细分结构�?
enum class PlanetWireMode : int {
    None = 0,
    Ocean = 1,
    BakedLod = 2,
    MountainMask = 3
};

enum class TerrainFeatureOverlayMode : int {
    None = 0,
    All = 1,
    Rivers = 2,
    Coast = 3,
    Ridges = 4,
    Erosion = 5
};

// 所有可调渲�?生成参数�?
// 这一个结构同时服�?UI、CPU 生成、GPU uniform 上传�?session 保存�?
struct PlanetRenderSettings {
    // 星球几何尺度�?tessellation LOD�?
    float planetRadius = 200.0f;
    float seaLevelOffset = 0.0f;
    float oceanTessellationMax = 1.0f;
    float oceanTessellationMin = 1.0f;
    float oceanTessellationNearDistance = 40.0f;
    float oceanTessellationFarDistance = 550.0f;
    // 程序化地形和侵蚀参数�?
    float terrainHeightScale = 22.0f;
    float terrainNoiseScale = 0.58f;
    float mountainMaskStrength = 0.55f;
    float mountainMaskScale = 1.8f;
    float mountainRidgeSharpness = 2.6f;
    int erosionIterations = 0;
    float erosionStrength = 0.0f;
    float erosionTalus = 0.028f;
    float erosionSediment = 0.58f;
    float erosionThermalStrength = 0.0f;
    // 地表材质颜色�?biome/slope 阈值�?
    glm::vec3 terrainLowlandColor = glm::vec3(0.23f, 0.44f, 0.18f);
    glm::vec3 terrainForestColor = glm::vec3(0.10f, 0.30f, 0.12f);
    glm::vec3 terrainDesertColor = glm::vec3(0.70f, 0.57f, 0.32f);
    glm::vec3 terrainRockColor = glm::vec3(0.42f, 0.38f, 0.32f);
    glm::vec3 terrainBeachColor = glm::vec3(0.72f, 0.66f, 0.46f);
    glm::vec3 terrainSnowColor = glm::vec3(0.90f, 0.94f, 0.98f);
    float terrainBeachWidth = 0.045f;
    float terrainRockSlopeStart = 0.24f;
    float terrainRockSlopeEnd = 0.62f;
    float terrainSnowStart = 0.72f;
    float terrainSnowEnd = 0.95f;
    float terrainMaterialNoiseScale = 0.030f;
    float terrainMaterialNoiseStrength = 0.0f;
    bool renderRivers = true;
    float riverVisibility = 1.45f;
    float riverWidth = 0.58f;
    float riverShine = 1.05f;
    float riverRefractionStrength = 0.48f;
    glm::vec3 riverColor = glm::vec3(0.02f, 0.36f, 0.42f);
    float coarseGridLineWidth = 1.6f;
    // 天空、大气和相机裁剪面�?
    glm::vec3 skyColor = glm::vec3(0.0f);
    float fogDensity = 0.0f;
    bool renderAtmosphere = true;
    float atmosphereHeight = 28.0f;
    float atmosphereDensity = 1.0f;
    float atmosphereRayleighStrength = 1.25f;
    float atmosphereMieStrength = 0.32f;
    float atmosphereMieAnisotropy = 0.76f;
    float atmosphereExposure = 1.15f;
    glm::vec3 atmosphereRayleighColor = glm::vec3(0.32f, 0.56f, 1.0f);
    glm::vec3 atmosphereMieColor = glm::vec3(1.0f, 0.72f, 0.42f);
    bool renderClouds = true;
    float cloudCoverage = 0.46f;
    float cloudSharpness = 1.35f;
    float cloudScale = 4.2f;
    float cloudSpeed = 0.018f;
    float cloudRotationSpeed = 1.5f;
    float cloudHeight = 12.0f;
    float cloudOpacity = 0.62f;
    glm::vec3 cloudColor = glm::vec3(0.96f, 0.98f, 1.0f);
    float cameraNearPlane = 1.0f;
    float cameraFarPlane = 5000.0f;
    // 海水透明度、颜色、反射折射、波浪和材质参数�?
    float oceanAlpha = 0.96f;
    float oceanShallowAlpha = 0.48f;
    float oceanDeepAlpha = 0.98f;
    float oceanFresnelStrength = 1.30f;
    float oceanDistortionStrength = 0.025f;
    float oceanDepthRange = 4.0f;
    float oceanShallowDepthRange = 0.45f;
    float oceanDepthScale = 6.0f;
    float oceanTintStrength = 0.02f;
    bool renderOceanWaves = true;
    bool renderOceanMaterial = true;
    float oceanWaveAmplitude = 0.18f;
    float oceanChoppiness = 0.10f;
    float oceanWaveTileScale = 4.0f;
    float oceanWaveNormalStrength = 1.0f;
    float oceanDetailNormalStrength = 0.22f;
    float oceanDetailNormalScale = 58.0f;
    float oceanDetailFadeDistance = 1080.0f;
    float oceanSpecularStrength = 0.35f;
    float oceanSpecularSharpness = 1.40f;
    float oceanRoughness = 0.29f;
    float oceanSSSStrength = 0.26f;
    float oceanSSSPower = 3.0f;
    float oceanShoreBlendWidth = 0.08f;
    bool renderOceanReflectionRefraction = true;
    bool renderOceanReflection = true;
    bool renderOceanRefraction = true;
    float oceanReflectionResolutionScale = 0.5f;
    int oceanReflectionFrameStride = 2;
    int oceanRefractionFrameStride = 2;
    bool oceanAutoDistanceLod = true;
    float oceanReflectionMaxAltitude = 1200.0f;
    float oceanRefractionMaxAltitude = 450.0f;
    int oceanFftCascadeCount = 1;
    int oceanFftFrameStride = 4;
    glm::vec3 oceanShallowColor = glm::vec3(0.0f, 0.20f, 0.31f);
    glm::vec3 oceanDeepColor = glm::vec3(0.01f, 0.06f, 0.18f);
    glm::vec3 oceanSSSColor = glm::vec3(0.18f, 0.82f, 0.78f);
    PlanetRenderMode renderMode = PlanetRenderMode::Shaded;
    PlanetWireMode wireMode = PlanetWireMode::None;
    TerrainFeatureOverlayMode featureOverlayMode = TerrainFeatureOverlayMode::None;
    bool renderTerrain = true;
    bool renderOcean = true;
};

// 渲染器负责：
// 1) 管理 shader、mesh、FBO �?GPU texture�?
// 2) 每帧 CPU 四叉�?LOD/裁剪�?
// 3) �?terrain/ocean/atmosphere/wire 顺序提交 draw call�?
class PlanetRenderer
{
public:
    // CPU LOD 阶段的统计信息，用于 performance 面板�?
    struct CullingStats {
        std::size_t visitedNodes = 0;
        std::size_t frustumCulledNodes = 0;
        std::size_t horizonCulledNodes = 0;
        std::size_t splitNodes = 0;
        std::size_t emittedPatches = 0;
    };

    // 每帧各渲染阶段耗时和动态质量参数�?
    struct PerformanceStats {
        float totalMs = 0.0f;
        float cullingMs = 0.0f;
        float fftMs = 0.0f;
        float reflectionRefractionMs = 0.0f;
        float terrainMs = 0.0f;
        float oceanMs = 0.0f;
        float atmosphereMs = 0.0f;
        float wireMs = 0.0f;
        bool fftUpdated = false;
        int fftCascadeCount = 0;
        int fftFrameStride = 1;
        bool reflectionUpdated = false;
        bool refractionUpdated = false;
        bool reflectionEnabled = false;
        bool refractionEnabled = false;
        int reflectionFrameStride = 1;
        int refractionFrameStride = 1;
        float reflectionWeight = 0.0f;
        float refractionWeight = 0.0f;
        std::size_t oceanPatchCount = 0;
        std::size_t bakedChunkCount = 0;
        std::size_t visibleBakedChunkCount = 0;
        std::array<std::size_t, 3> visibleBakedChunkLodCount{};
        float lodSplitPixelScale = 1.0f;
        float effectiveOceanTessMax = 1.0f;
        std::size_t estimatedTerrainTriangles = 0;
        std::size_t estimatedOceanTriangles = 0;
    };

    PlanetRenderer();

    void initialize();
    void setPlanetRotation(float yawDegrees, float pitchDegrees);
    void setProceduralData(const PlanetProceduralData& proceduralData);

    PlanetRenderSettings& settings();
    const PlanetRenderSettings& settings() const;

    void render(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float timeSeconds);

    const char* currentModeLabel() const;
    std::size_t visibleOceanPatchCount() const;
    const CullingStats& cullingStats() const;
    const PerformanceStats& performanceStats() const;

private:
    // cube face 局部坐标系，与 PlanetProceduralData 中的定义保持一致�?
    struct FaceBasis {
        glm::vec3 normal;
        glm::vec3 axisU;
        glm::vec3 axisV;
    };

    // CPU 四叉树节点，使用 cube face UV 范围描述 patch�?
    struct QuadtreeNode {
        glm::vec2 uvMin{0.0f, 0.0f};
        float uvSize = 1.0f;
        int depth = 0;
    };

    // 一�?patch 内水/�?海岸覆盖情况，来�?CPU prefix sum 快速查询�?
    struct PatchWaterCoverage {
        bool hasData = false;
        bool hasWater = false;
        bool hasLand = false;
        float maxShoreMask = 0.0f;
    };

    // 每帧实际提交�?shader �?patch�?
    struct OceanPatch {
        int faceIndex = 0;
        glm::vec2 uvMin{0.0f, 0.0f};
        glm::vec2 uvSize{1.0f, 1.0f};
        int depth = 0;
        PatchWaterCoverage waterCoverage;
    };

    // �?view-projection 矩阵提取出的 6 个裁剪平面�?
    struct Frustum {
        std::array<glm::vec4, 6> planes{};
    };

    // LOD/裁剪使用的球面近似包围体�?
    struct NodeBounds {
        glm::vec3 worldDirection{0.0f, 1.0f, 0.0f};
        float radius = 0.001f;
        float lodScale = 1.0f;
    };

    // 所有地形和海洋 patch 共享同一个规则网�?VAO，由 tessellation shader 放大�?
    struct TerrainMesh {
        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLuint indexBufferObject = 0;
        GLsizei indexCount = 0;

        void buildGrid(int patchResolution);
        void draw() const;
    };

    struct VisibleBakedChunk {
        std::uint32_t chunkIndex = 0;
        std::uint8_t lod = 0;
    };

    struct BakedTerrainMesh {
        struct IndexRange {
            GLsizei indexCount = 0;
            std::size_t firstIndex = 0;
            std::size_t triangleCount = 0;
        };

        struct ChunkDrawRange {
            std::array<IndexRange, 3> lods{};
            glm::vec3 localCenter{0.0f};
            float radius = 0.0f;
        };

        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLuint indexBufferObject = 0;
        GLuint lineVertexArrayObject = 0;
        GLuint lineVertexBufferObject = 0;
        GLsizei indexCount = 0;
        std::vector<ChunkDrawRange> chunks;

        void release();
        void upload(const PlanetProceduralData& proceduralData, float planetRadius, float heightScale);
        void draw(const std::vector<VisibleBakedChunk>& visibleChunks) const;
        void drawWire(const std::vector<VisibleBakedChunk>& visibleChunks) const;
        void drawChunkBounds(const std::vector<VisibleBakedChunk>& visibleChunks) const;
    };

    // 大气层使用普通球体网格，不走 tessellation�?
    struct SphereMesh {
        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLuint indexBufferObject = 0;
        GLsizei indexCount = 0;

        void buildSphere(int longitudeSegments, int latitudeSegments);
        void draw() const;
    };

    // 反射/折射离屏渲染目标�?
    struct RenderTarget {
        GLuint framebufferObject = 0;
        GLuint colorTexture = 0;
        GLuint depthTexture = 0;
        int width = 0;
        int height = 0;

        void release();
        void create(int targetWidth, int targetHeight);
    };

    struct FeatureSegmentMesh {
        struct TypeRange {
            GLsizei vertexCount = 0;
            std::size_t firstVertex = 0;
        };

        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLsizei vertexCount = 0;
        std::array<TypeRange, 4> ranges{};

        void release();
        void upload(const PlanetProceduralData& proceduralData, float planetRadius, float heightScale);
        void draw(TerrainFeatureOverlayMode mode) const;
    };

    static constexpr int kNodeGridResolution = 10;
    static constexpr int kMinimumLodDepth = 1;
    static constexpr int kMaximumLodDepth = 7;
    static constexpr int kShoreMinimumLodDepth = 4;
    static constexpr float kLodSplitPixels = 130.0f;

    static const std::array<FaceBasis, 6> kPlanetFaces;

    PlanetRenderSettings settings_;
    TerrainMesh terrainMesh_;
    BakedTerrainMesh bakedTerrainMesh_;
    FeatureSegmentMesh featureSegmentMesh_;
    SphereMesh atmosphereMesh_;
    RenderTarget reflectionTarget_;
    RenderTarget refractionTarget_;
    FFTOcean fftOcean_;
    GLuint proceduralHeightTexture_ = 0;
    GLuint proceduralWaterDepthTexture_ = 0;
    GLuint proceduralErosionMaskTexture_ = 0;
    GLuint proceduralTemperatureTexture_ = 0;
    GLuint proceduralMoistureTexture_ = 0;
    GLuint proceduralBiomeWeightATexture_ = 0;
    GLuint proceduralBiomeWeightBTexture_ = 0;
    GLuint proceduralDomainWeightTexture_ = 0;
    std::vector<std::uint32_t> proceduralWaterCoveragePrefixCpu_;
    std::vector<std::uint32_t> proceduralShoreCoverageLoosePrefixCpu_;
    std::vector<std::uint32_t> proceduralShoreCoverageStrictPrefixCpu_;
    int proceduralDataResolution_ = 0;
    bool hasProceduralOceanData_ = false;
    ShaderProgram terrainChunkProgram_;
    ShaderProgram bakedChunkBoundsProgram_;
    ShaderProgram featureSegmentProgram_;
    ShaderProgram oceanProgram_;
    ShaderProgram oceanWireOverlayProgram_;
    ShaderProgram oceanCoarseGridProgram_;
    ShaderProgram atmosphereProgram_;
    glm::mat4 modelMatrix_;
    glm::vec3 lightDirection_;
    float currentTimeSeconds_ = 0.0f;
    float currentDeltaSeconds_ = 1.0f / 60.0f;
    float lastRenderTimeSeconds_ = 0.0f;
    float planetYawDegrees_ = 0.0f;
    float planetPitchDegrees_ = 0.0f;
    std::vector<OceanPatch> visibleOceanPatches_;
    std::vector<VisibleBakedChunk> visibleBakedChunks_;
    CullingStats lastCullingStats_;
    PerformanceStats lastPerformanceStats_;
    int oceanFftFrameCounter_ = 0;
    int oceanReflectionFrameCounter_ = 0;
    int oceanRefractionFrameCounter_ = 0;
    bool lastReflectionUpdated_ = false;
    bool lastRefractionUpdated_ = false;
    bool lastReflectionEnabled_ = false;
    bool lastRefractionEnabled_ = false;
    float oceanReflectionWeight_ = 1.0f;
    float oceanRefractionWeight_ = 1.0f;
    float lodSplitPixelScale_ = 1.0f;
    float effectiveOceanTessellationMax_ = 1.0f;
    bool hasLastRenderTimeSeconds_ = false;
    bool initialized_ = false;

    static glm::vec3 cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv);
    static int faceIndexFromDirection(const glm::vec3& direction);
    static glm::vec2 faceUvFromDirection(int faceIndex, const glm::vec3& direction);
    static glm::vec3 nodeCenterDirection(const FaceBasis& face, const QuadtreeNode& node);
    static Frustum extractFrustum(const glm::mat4& viewProjectionMatrix);
    static glm::vec4 normalizePlane(const glm::vec4& plane);
    glm::vec3 worldDirection(const glm::vec3& localDirection) const;
    // 下面一组函数组�?CPU 可见�?LOD 流程�?
    NodeBounds computeNodeBounds(const FaceBasis& face, const QuadtreeNode& node) const;
    bool isNodeOutsideFrustum(const Frustum& frustum, const NodeBounds& bounds) const;
    bool isNodeHiddenByHorizon(const FlyCamera& camera, const NodeBounds& bounds) const;
    bool isSphereOutsideFrustum(const Frustum& frustum, const glm::vec3& worldCenter, float radius) const;
    bool isSphereHiddenByHorizon(const FlyCamera& camera, const glm::vec3& worldCenter, float radius) const;
    PatchWaterCoverage analyzePatchWaterCoverage(int faceIndex, const glm::vec2& uvMin, const glm::vec2& uvSize) const;
    bool shouldSplitNode(const FlyCamera& camera, const NodeBounds& bounds, int nodeDepth, int framebufferHeight) const;
    void updateOceanTessellationBudget(std::size_t oceanPatchCount);
    std::size_t estimatePatchTriangles(std::size_t patchCount, float tessLevel) const;

    void collectVisibleOceanPatches(const FlyCamera& camera,
                                    const Frustum& frustum,
                                    int faceIndex,
                                    const QuadtreeNode& node,
                                    int framebufferHeight,
                                    CullingStats& stats,
                                    std::vector<OceanPatch>& outPatches) const;

    std::vector<VisibleBakedChunk> buildVisibleBakedChunks(const FlyCamera& camera,
                                                           const Frustum& frustum,
                                                           int framebufferHeight,
                                                           CullingStats& stats) const;
    std::vector<OceanPatch> buildVisibleOceanPatches(const FlyCamera& camera,
                                                     const Frustum& frustum,
                                                     int framebufferHeight,
                                                     CullingStats& stats) const;
    bool patchHasOceanCoverage(const OceanPatch& patch) const;

    // �?terrain/ocean/wire shader 统一上传当前 patch 和全局渲染参数�?
    void applyCommonUniforms(const ShaderProgram& program,
                             const FlyCamera& camera,
                             const glm::mat4& viewMatrix,
                             const glm::mat4& projectionMatrix,
                             const OceanPatch& patch) const;

    float seaLevelRadius() const;

    // 渲染 pass。反�?折射 pass 会复�?terrain pass 并启用裁剪平面�?
    void drawTerrainPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void drawTerrainPass(const FlyCamera& camera,
                         const glm::mat4& viewMatrix,
                         const glm::mat4& projectionMatrix,
                         bool useClipPlane,
                         float clipPlaneY,
                         bool keepAboveClipPlane);
    void drawBakedTerrainPass(const FlyCamera& camera,
                              const glm::mat4& viewMatrix,
                              const glm::mat4& projectionMatrix,
                              bool useClipPlane,
                              float clipPlaneY,
                              bool keepAboveClipPlane);
    void drawOceanPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void drawAtmospherePass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void drawReflectionRefractionPasses(const FlyCamera& camera,
                                        const glm::mat4& viewMatrix,
                                        const glm::mat4& projectionMatrix,
                                        int framebufferWidth,
                                        int framebufferHeight);
    void drawWireOverlayPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void drawFeatureOverlayPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
};
