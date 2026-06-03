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

// 鍦板舰璋冭瘯/灞曠ず妯″紡銆?
enum class PlanetRenderMode : int {
    Shaded = 0,
    Unshaded = 1,
    HeightMap = 2,
    Normals = 3
};

// 绾挎鍙犲姞妯″紡锛氬彲閫夋嫨鏄剧ず闄嗗湴 patch 鎴栨捣娲?patch 鐨勭粏鍒嗙粨鏋勩€?
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

// 鎵€鏈夊彲璋冩覆鏌?鐢熸垚鍙傛暟銆?
// 杩欎竴涓粨鏋勫悓鏃舵湇鍔?UI銆丆PU 鐢熸垚銆丟PU uniform 涓婁紶鍜?session 淇濆瓨銆?
struct PlanetRenderSettings {
    // 鏄熺悆鍑犱綍灏哄害涓?tessellation LOD銆?
    float planetRadius = 200.0f;
    float seaLevelOffset = 0.0f;
    float oceanTessellationMax = 1.0f;
    float oceanTessellationMin = 1.0f;
    float oceanTessellationNearDistance = 40.0f;
    float oceanTessellationFarDistance = 550.0f;
    // 绋嬪簭鍖栧湴褰㈠拰渚佃殌鍙傛暟銆?
    float terrainHeightScale = 22.0f;
    float runtimeMountainScale = 1.0f;
    float terrainNoiseScale = 0.58f;
    float mountainMaskStrength = 1.90f;
    float mountainMaskScale = 3.35f;
    float mountainRidgeSharpness = 3.80f;
    int erosionIterations = 128;
    float erosionStrength = 0.075f;
    float erosionTalus = 0.028f;
    float erosionSediment = 0.58f;
    float erosionThermalStrength = 0.018f;
    // 鍦拌〃鏉愯川棰滆壊涓?biome/slope 闃堝€笺€?
    glm::vec3 terrainLowlandColor = glm::vec3(0.20f, 0.46f, 0.30f);
    glm::vec3 terrainForestColor = glm::vec3(0.08f, 0.28f, 0.18f);
    glm::vec3 terrainDesertColor = glm::vec3(0.64f, 0.55f, 0.34f);
    glm::vec3 terrainRockColor = glm::vec3(0.44f, 0.47f, 0.48f);
    glm::vec3 terrainBeachColor = glm::vec3(0.79f, 0.68f, 0.43f);
    glm::vec3 terrainSnowColor = glm::vec3(0.94f, 0.97f, 0.98f);
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
    // 澶╃┖銆佸ぇ姘斿拰鐩告満瑁佸壀闈€?
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
    // 娴锋按閫忔槑搴︺€侀鑹层€佸弽灏勬姌灏勩€佹尝娴拰鏉愯川鍙傛暟銆?
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

// 娓叉煋鍣ㄨ礋璐ｏ細
// 1) 绠＄悊 shader銆乵esh銆丗BO 鍜?GPU texture锛?
// 2) 姣忓抚 CPU 鍥涘弶鏍?LOD/瑁佸壀锛?
// 3) 鎸?terrain/ocean/atmosphere/wire 椤哄簭鎻愪氦 draw call銆?
class PlanetRenderer
{
public:
    // CPU LOD 闃舵鐨勭粺璁′俊鎭紝鐢ㄤ簬 performance 闈㈡澘銆?
    struct CullingStats {
        std::size_t visitedNodes = 0;
        std::size_t frustumCulledNodes = 0;
        std::size_t horizonCulledNodes = 0;
        std::size_t splitNodes = 0;
        std::size_t emittedPatches = 0;
    };

    // 姣忓抚鍚勬覆鏌撻樁娈佃€楁椂鍜屽姩鎬佽川閲忓弬鏁般€?
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
    // cube face 灞€閮ㄥ潗鏍囩郴锛屼笌 PlanetProceduralData 涓殑瀹氫箟淇濇寔涓€鑷淬€?
    struct FaceBasis {
        glm::vec3 normal;
        glm::vec3 axisU;
        glm::vec3 axisV;
    };

    // CPU 鍥涘弶鏍戣妭鐐癸紝浣跨敤 cube face UV 鑼冨洿鎻忚堪 patch銆?
    struct QuadtreeNode {
        glm::vec2 uvMin{0.0f, 0.0f};
        float uvSize = 1.0f;
        int depth = 0;
    };

    // 涓€涓?patch 鍐呮按/闄?娴峰哺瑕嗙洊鎯呭喌锛屾潵鑷?CPU prefix sum 蹇€熸煡璇€?
    struct PatchWaterCoverage {
        bool hasData = false;
        bool hasWater = false;
        bool hasLand = false;
        float maxShoreMask = 0.0f;
    };

    // 姣忓抚瀹為檯鎻愪氦缁?shader 鐨?patch銆?
    struct OceanPatch {
        int faceIndex = 0;
        glm::vec2 uvMin{0.0f, 0.0f};
        glm::vec2 uvSize{1.0f, 1.0f};
        int depth = 0;
        PatchWaterCoverage waterCoverage;
    };

    // 浠?view-projection 鐭╅樀鎻愬彇鍑虹殑 6 涓鍓钩闈€?
    struct Frustum {
        std::array<glm::vec4, 6> planes{};
    };

    // LOD/瑁佸壀浣跨敤鐨勭悆闈㈣繎浼煎寘鍥翠綋銆?
    struct NodeBounds {
        glm::vec3 worldDirection{0.0f, 1.0f, 0.0f};
        float radius = 0.001f;
        float lodScale = 1.0f;
    };

    // 鎵€鏈夊湴褰㈠拰娴锋磱 patch 鍏变韩鍚屼竴涓鍒欑綉鏍?VAO锛岀敱 tessellation shader 鏀惧ぇ銆?
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

    // 澶ф皵灞備娇鐢ㄦ櫘閫氱悆浣撶綉鏍硷紝涓嶈蛋 tessellation銆?
    struct SphereMesh {
        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLuint indexBufferObject = 0;
        GLsizei indexCount = 0;

        void buildSphere(int longitudeSegments, int latitudeSegments);
        void draw() const;
    };

    // 鍙嶅皠/鎶樺皠绂诲睆娓叉煋鐩爣銆?
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
    // 涓嬮潰涓€缁勫嚱鏁扮粍鎴?CPU 鍙鎬?LOD 娴佺▼銆?
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

    // 缁?terrain/ocean/wire shader 缁熶竴涓婁紶褰撳墠 patch 鍜屽叏灞€娓叉煋鍙傛暟銆?
    void applyCommonUniforms(const ShaderProgram& program,
                             const FlyCamera& camera,
                             const glm::mat4& viewMatrix,
                             const glm::mat4& projectionMatrix,
                             const OceanPatch& patch) const;

    float seaLevelRadius() const;

    // 娓叉煋 pass銆傚弽灏?鎶樺皠 pass 浼氬鐢?terrain pass 骞跺惎鐢ㄨ鍓钩闈€?
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
