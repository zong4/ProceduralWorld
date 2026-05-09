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

enum class PlanetRenderMode : int {
    Shaded = 0,
    Unshaded = 1,
    HeightMap = 2,
    Normals = 3
};

enum class PlanetWireMode : int {
    None = 0,
    Terrain = 1,
    Ocean = 2
};

struct PlanetRenderSettings {
    float planetRadius = 200.0f;
    float seaLevelOffset = 0.0f;
    float tessellationMax = 1.0f;
    float tessellationMin = 1.0f;
    float tessellationNearDistance = 80.0f;
    float tessellationFarDistance = 900.0f;
    float oceanTessellationMax = 1.0f;
    float oceanTessellationMin = 1.0f;
    float oceanTessellationNearDistance = 40.0f;
    float oceanTessellationFarDistance = 550.0f;
    float terrainHeightScale = 18.0f;
    float terrainSkirtDepth = 0.40f;
    float terrainNoiseScale = 0.5f;
    float mountainMaskStrength = 1.25f;
    float mountainMaskScale = 2.4f;
    float mountainRidgeSharpness = 2.6f;
    int erosionIterations = 48;
    float erosionStrength = 0.045f;
    float erosionTalus = 0.028f;
    float erosionSediment = 0.58f;
    float erosionThermalStrength = 0.014f;
    float regionalDetailStrength = 0.95f;
    float microDetailStrength = 0.10f;
    float regionalDetailStartAltitude = 900.0f;
    float regionalDetailEndAltitude = 2200.0f;
    float microDetailStartAltitude = 90.0f;
    float microDetailEndAltitude = 520.0f;
    glm::vec3 terrainLowlandColor = glm::vec3(0.23f, 0.44f, 0.18f);
    glm::vec3 terrainForestColor = glm::vec3(0.10f, 0.30f, 0.12f);
    glm::vec3 terrainDesertColor = glm::vec3(0.70f, 0.57f, 0.32f);
    glm::vec3 terrainRockColor = glm::vec3(0.42f, 0.38f, 0.32f);
    glm::vec3 terrainBeachColor = glm::vec3(0.72f, 0.66f, 0.46f);
    glm::vec3 terrainSnowColor = glm::vec3(0.90f, 0.94f, 0.98f);
    float terrainBeachWidth = 0.045f;
    float terrainShoreLift = 0.035f;
    float terrainRockSlopeStart = 0.24f;
    float terrainRockSlopeEnd = 0.62f;
    float terrainSnowStart = 0.72f;
    float terrainSnowEnd = 0.95f;
    float terrainMaterialNoiseScale = 0.030f;
    float terrainMaterialNoiseStrength = 0.14f;
    float coarseGridLineWidth = 1.6f;
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
    float oceanWaveAmplitude = 0.0f;
    float oceanChoppiness = 0.0f;
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
    int terrainMaskDebugMode = 0;
    bool renderTerrain = true;
    bool renderOcean = true;
};

class PlanetRenderer
{
public:
    struct CullingStats {
        std::size_t visitedNodes = 0;
        std::size_t frustumCulledNodes = 0;
        std::size_t horizonCulledNodes = 0;
        std::size_t splitNodes = 0;
        std::size_t emittedPatches = 0;
    };

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
    };

    PlanetRenderer();

    void initialize();
    void setPlanetRotation(float yawDegrees, float pitchDegrees);
    void setProceduralData(const PlanetProceduralData& proceduralData);

    PlanetRenderSettings& settings();
    const PlanetRenderSettings& settings() const;

    void render(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, float timeSeconds);

    const char* currentModeLabel() const;
    std::size_t visiblePatchCount() const;
    const CullingStats& cullingStats() const;
    const PerformanceStats& performanceStats() const;

private:
    struct FaceBasis {
        glm::vec3 normal;
        glm::vec3 axisU;
        glm::vec3 axisV;
    };

    struct QuadtreeNode {
        glm::vec2 uvMin{0.0f, 0.0f};
        float uvSize = 1.0f;
        int depth = 0;
    };

    struct PatchWaterCoverage {
        bool hasData = false;
        bool hasWater = false;
        bool hasLand = false;
        float maxShoreMask = 0.0f;
    };

    struct RenderPatch {
        int faceIndex = 0;
        glm::vec2 uvMin{0.0f, 0.0f};
        glm::vec2 uvSize{1.0f, 1.0f};
        int depth = 0;
        PatchWaterCoverage waterCoverage;
    };

    struct Frustum {
        std::array<glm::vec4, 6> planes{};
    };

    struct NodeBounds {
        glm::vec3 worldDirection{0.0f, 1.0f, 0.0f};
        float radius = 0.001f;
    };

    struct TerrainMesh {
        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLuint indexBufferObject = 0;
        GLsizei indexCount = 0;

        void buildGrid(int patchResolution);
        void draw() const;
    };

    struct SphereMesh {
        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLuint indexBufferObject = 0;
        GLsizei indexCount = 0;

        void buildSphere(int longitudeSegments, int latitudeSegments);
        void draw() const;
    };

    struct RenderTarget {
        GLuint framebufferObject = 0;
        GLuint colorTexture = 0;
        GLuint depthTexture = 0;
        int width = 0;
        int height = 0;

        void release();
        void create(int targetWidth, int targetHeight);
    };

    static constexpr int kNodeGridResolution = 8;
    static constexpr int kMinimumLodDepth = 1;
    static constexpr int kMaximumLodDepth = 6;
    static constexpr int kShoreMinimumLodDepth = 4;
    static constexpr float kLodSplitPixels = 190.0f;

    static const std::array<FaceBasis, 6> kPlanetFaces;

    PlanetRenderSettings settings_;
    TerrainMesh terrainMesh_;
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
    std::vector<std::uint32_t> proceduralWaterCoveragePrefixCpu_;
    std::vector<std::uint32_t> proceduralShoreCoverageLoosePrefixCpu_;
    std::vector<std::uint32_t> proceduralShoreCoverageStrictPrefixCpu_;
    int proceduralDataResolution_ = 0;
    bool hasProceduralOceanData_ = false;
    ShaderProgram terrainProgram_;
    ShaderProgram oceanProgram_;
    ShaderProgram wireOverlayProgram_;
    ShaderProgram coarseGridProgram_;
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
    std::vector<RenderPatch> visiblePatches_;
    std::vector<RenderPatch> visibleOceanPatches_;
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
    bool hasLastRenderTimeSeconds_ = false;
    bool initialized_ = false;

    static glm::vec3 cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv);
    static int faceIndexFromDirection(const glm::vec3& direction);
    static glm::vec2 faceUvFromDirection(int faceIndex, const glm::vec3& direction);
    static glm::vec3 nodeCenterDirection(const FaceBasis& face, const QuadtreeNode& node);
    static Frustum extractFrustum(const glm::mat4& viewProjectionMatrix);
    static glm::vec4 normalizePlane(const glm::vec4& plane);
    glm::vec3 worldDirection(const glm::vec3& localDirection) const;
    NodeBounds computeNodeBounds(const FaceBasis& face, const QuadtreeNode& node) const;
    bool isNodeOutsideFrustum(const Frustum& frustum, const NodeBounds& bounds) const;
    bool isNodeHiddenByHorizon(const FlyCamera& camera, const NodeBounds& bounds) const;
    PatchWaterCoverage analyzePatchWaterCoverage(int faceIndex, const glm::vec2& uvMin, const glm::vec2& uvSize) const;
    bool shouldSplitNode(const FlyCamera& camera, const NodeBounds& bounds, int nodeDepth, int framebufferHeight) const;

    void collectVisiblePatches(const FlyCamera& camera,
                               const Frustum& frustum,
                               int faceIndex,
                               const QuadtreeNode& node,
                               int framebufferHeight,
                               CullingStats& stats,
                               std::vector<RenderPatch>& outPatches) const;

    std::vector<RenderPatch> buildVisiblePatches(const FlyCamera& camera,
                                                 const Frustum& frustum,
                                                 int framebufferHeight);
    std::vector<RenderPatch> buildVisibleOceanPatches() const;
    bool patchHasOceanCoverage(const RenderPatch& patch) const;

    void applyCommonUniforms(const ShaderProgram& program,
                             const FlyCamera& camera,
                             const glm::mat4& viewMatrix,
                             const glm::mat4& projectionMatrix,
                             const RenderPatch& patch) const;

    float seaLevelRadius() const;

    void drawTerrainPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void drawTerrainPass(const FlyCamera& camera,
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
};
