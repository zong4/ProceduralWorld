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

// Terrain debug and visualization modes.
enum class PlanetRenderMode : int {
    Shaded = 0,
    Unshaded = 1,
    HeightMap = 2,
    Normals = 3,
    Material = 4
};

// Optional wire/debug overlays for terrain and ocean geometry.
enum class PlanetWireMode : int {
    None = 0,
    Ocean = 1,
    BakedLod = 2,
    MountainMask = 3
};

// CPU-generated feature overlays drawn over the terrain.
enum class TerrainFeatureOverlayMode : int {
    None = 0,
    All = 1,
    Rivers = 2,
    Coast = 3,
    Ridges = 4,
    Erosion = 5
};

// Shared render and procedural settings.
// UI, CPU generation, GPU uniforms, and session persistence all use this struct.
struct PlanetRenderSettings {
    float planetRadius = 200.0f;
    float seaLevelOffset = 0.0f;
    float oceanTessellationMax = 1.0f;
    float oceanTessellationMin = 1.0f;
    float oceanTessellationNearDistance = 40.0f;
    float oceanTessellationFarDistance = 550.0f;
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
    glm::vec3 terrainLowlandColor = glm::vec3(0.20f, 0.46f, 0.30f);
    glm::vec3 terrainForestColor = glm::vec3(0.08f, 0.28f, 0.18f);
    glm::vec3 terrainDesertColor = glm::vec3(0.64f, 0.55f, 0.34f);
    glm::vec3 terrainRockColor = glm::vec3(0.44f, 0.47f, 0.48f);
    glm::vec3 terrainBeachColor = glm::vec3(0.79f, 0.68f, 0.43f);
    glm::vec3 terrainSnowColor = glm::vec3(0.94f, 0.97f, 0.98f);
    glm::vec3 terrainPaletteLowGrass = glm::vec3(0.20f, 0.86f, 0.18f);
    glm::vec3 terrainPaletteMeadow = glm::vec3(0.56f, 0.90f, 0.16f);
    glm::vec3 terrainPaletteForestDark = glm::vec3(0.020f, 0.38f, 0.070f);
    glm::vec3 terrainPaletteForestWarm = glm::vec3(0.12f, 0.64f, 0.10f);
    glm::vec3 terrainPaletteSavanna = glm::vec3(0.92f, 0.72f, 0.12f);
    glm::vec3 terrainPaletteDrySoil = glm::vec3(0.94f, 0.42f, 0.12f);
    glm::vec3 terrainPaletteOchre = glm::vec3(1.00f, 0.60f, 0.16f);
    glm::vec3 terrainPaletteWetGreen = glm::vec3(0.030f, 0.52f, 0.30f);
    glm::vec3 terrainPaletteTundra = glm::vec3(0.70f, 0.78f, 0.32f);
    glm::vec3 terrainPaletteBrownSlope = glm::vec3(0.58f, 0.36f, 0.20f);
    glm::vec3 terrainPaletteRedSoil = glm::vec3(0.70f, 0.30f, 0.14f);
    glm::vec3 terrainPaletteRockWarm = glm::vec3(0.48f, 0.45f, 0.39f);
    glm::vec3 terrainPaletteRockCool = glm::vec3(0.54f, 0.58f, 0.56f);
    glm::vec3 terrainPaletteRockDark = glm::vec3(0.22f, 0.22f, 0.20f);
    glm::vec3 terrainPalettePaleStone = glm::vec3(0.70f, 0.72f, 0.66f);
    glm::vec3 terrainPaletteSnow = glm::vec3(1.08f, 1.10f, 1.14f);
    glm::vec3 terrainPaletteSnowShadow = glm::vec3(0.76f, 0.86f, 1.02f);
    glm::vec3 terrainPaletteBeach = glm::vec3(1.00f, 0.78f, 0.38f);
    glm::vec3 terrainPaletteRiverBed = glm::vec3(0.30f, 0.17f, 0.08f);
    glm::vec3 terrainPaletteShallowSeabed = glm::vec3(0.28f, 0.62f, 0.50f);
    glm::vec3 terrainPaletteDeepSeabed = glm::vec3(0.08f, 0.22f, 0.32f);
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
    glm::vec3 skyColor = glm::vec3(0.0f);
    float fogDensity = 0.0f;
    bool renderAtmosphere = true;
    float atmosphereHeight = 14.0f;
    float atmosphereDensity = 1.0f;
    float atmosphereRayleighStrength = 1.25f;
    float atmosphereMieStrength = 0.32f;
    float atmosphereMieAnisotropy = 0.76f;
    float atmosphereExposure = 1.15f;
    glm::vec3 atmosphereRayleighColor = glm::vec3(0.32f, 0.56f, 1.0f);
    glm::vec3 atmosphereMieColor = glm::vec3(1.0f, 0.72f, 0.42f);
    bool renderClouds = true;
    float cloudCoverage = 0.36f;
    float cloudSharpness = 0.92f;
    float cloudScale = 1.85f;
    float cloudSpeed = 0.018f;
    float cloudHeight = 7.2f;
    float cloudThickness = 6.2f;
    float cloudDensity = 1.70f;
    float cloudShadowStrength = 1.02f;
    int cloudStepCount = 22;
    int cloudLightStepCount = 4;
    float cloudOpacity = 0.78f;
    glm::vec3 cloudColor = glm::vec3(0.96f, 0.98f, 1.0f);
    float cameraNearPlane = 1.0f;
    float cameraFarPlane = 5000.0f;
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
    int oceanReflectionFrameStride = 1;
    int oceanRefractionFrameStride = 1;
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

// Owns planet rendering resources, CPU LOD/culling, and frame draw submission.
class PlanetRenderer
{
public:
    // CPU LOD/culling counters shown in the performance panel.
    struct CullingStats {
        std::size_t visitedNodes = 0;
        std::size_t frustumCulledNodes = 0;
        std::size_t horizonCulledNodes = 0;
        std::size_t splitNodes = 0;
        std::size_t emittedPatches = 0;
    };

    // CPU-side frame timing and draw workload summary.
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
    // Cube-face basis. Keep in sync with PlanetProceduralData.
    struct FaceBasis {
        glm::vec3 normal;
        glm::vec3 axisU;
        glm::vec3 axisV;
    };

    // CPU quadtree node in cube-face UV space.
    struct QuadtreeNode {
        glm::vec2 uvMin{0.0f, 0.0f};
        float uvSize = 1.0f;
        int depth = 0;
    };

    // Water/land/shore coverage for a patch, queried from CPU prefix sums.
    struct PatchWaterCoverage {
        bool hasData = false;
        bool hasWater = false;
        bool hasLand = false;
        float maxShoreMask = 0.0f;
    };

    // Per-frame ocean patch submitted to shaders.
    struct OceanPatch {
        int faceIndex = 0;
        glm::vec2 uvMin{0.0f, 0.0f};
        glm::vec2 uvSize{1.0f, 1.0f};
        int depth = 0;
        PatchWaterCoverage waterCoverage;
    };

    // Six clipping planes extracted from a view-projection matrix.
    struct Frustum {
        std::array<glm::vec4, 6> planes{};
    };

    // Approximate spherical bounds used by LOD and culling.
    struct NodeBounds {
        glm::vec3 worldDirection{0.0f, 1.0f, 0.0f};
        float radius = 0.001f;
        float lodScale = 1.0f;
    };

    // Shared terrain/ocean patch grid; tessellation shaders expand it.
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
        float maxTerrainRadius = 0.0f;
        std::vector<ChunkDrawRange> chunks;

        void release();
        void upload(const PlanetProceduralData& proceduralData, float planetRadius, float heightScale);
        void draw(const std::vector<VisibleBakedChunk>& visibleChunks) const;
        void drawWire(const std::vector<VisibleBakedChunk>& visibleChunks) const;
        void drawChunkBounds(const std::vector<VisibleBakedChunk>& visibleChunks) const;
    };

    // Simple sphere mesh used by legacy/debug shell rendering.
    struct SphereMesh {
        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLuint indexBufferObject = 0;
        GLsizei indexCount = 0;

        void buildSphere(int longitudeSegments, int latitudeSegments);
        void draw() const;
    };

    // Offscreen reflection/refraction render target.
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

    struct AtmosphereLut {
        GLuint framebufferObject = 0;
        GLuint transmittanceTexture = 0;
        GLuint irradianceTexture = 0;
        GLuint scatteringTexture = 0;
        GLuint deltaScatteringTextureA = 0;
        GLuint deltaScatteringTextureB = 0;
        int transmittanceWidth = 256;
        int transmittanceHeight = 64;
        int irradianceWidth = 64;
        int irradianceHeight = 16;
        int scatteringViewMuSize = 32;
        int scatteringNuSize = 8;
        int scatteringWidth = 256;
        int scatteringHeight = 32;
        int scatteringDepth = 32;
        int scatteringOrderCount = 4;
        std::uint64_t cachedSignature = 0;

        void release();
        void create();
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
    AtmosphereLut atmosphereLut_;
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
    ShaderProgram atmosphereTransmittanceProgram_;
    ShaderProgram atmosphereIrradianceProgram_;
    ShaderProgram atmosphereScatteringProgram_;
    ShaderProgram atmosphereAccumulateProgram_;
    GLuint fullscreenVertexArrayObject_ = 0;
    GLuint atmosphereSceneDepthTexture_ = 0;
    int atmosphereSceneDepthWidth_ = 0;
    int atmosphereSceneDepthHeight_ = 0;
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
    // CPU-side LOD and visibility helpers.
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

    // Upload shared terrain/ocean/wire uniforms for the current patch.
    void applyCommonUniforms(const ShaderProgram& program,
                             const FlyCamera& camera,
                             const glm::mat4& viewMatrix,
                             const glm::mat4& projectionMatrix,
                             const OceanPatch& patch) const;

    float seaLevelRadius() const;

    // Render passes. Reflection/refraction reuse terrain rendering with clip planes.
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
    std::uint64_t computeAtmosphereLutSignature() const;
    void precomputeAtmosphereLuts();
    void copyAtmosphereSceneDepth(int framebufferWidth, int framebufferHeight);
    void drawAtmospherePass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void drawReflectionRefractionPasses(const FlyCamera& camera,
                                        const glm::mat4& viewMatrix,
                                        const glm::mat4& projectionMatrix,
                                        int framebufferWidth,
                                        int framebufferHeight);
    void drawWireOverlayPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void drawFeatureOverlayPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
};
