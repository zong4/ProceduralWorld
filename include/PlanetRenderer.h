#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "FlyCamera.h"
#include "ShaderProgram.h"

enum class PlanetRenderMode : int {
    Shaded = 0,
    HeightMap = 2,
    Normals = 3
};

struct PlanetRenderSettings {
    float planetRadius = 20.0f;
    float seaLevelOffset = 0.0f;
    float tessellationMax = 6.0f;
    float tessellationMin = 1.0f;
    float tessellationNearDistance = 8.0f;
    float tessellationFarDistance = 90.0f;
    float terrainHeightScale = 1.2f;
    float terrainNoiseScale = 2.8f;
    float regionalDetailStrength = 0.55f;
    float microDetailStrength = 0.22f;
    float regionalDetailStartAltitude = 18.0f;
    float regionalDetailEndAltitude = 120.0f;
    float microDetailStartAltitude = 6.0f;
    float microDetailEndAltitude = 42.0f;
    float coarseGridLineWidth = 1.6f;
    float oceanAlpha = 0.90f;
    float oceanFresnelStrength = 1.20f;
    float oceanDistortionStrength = 0.025f;
    float oceanDepthRange = 8.0f;
    glm::vec3 oceanShallowColor = glm::vec3(0.18f, 0.58f, 0.78f);
    glm::vec3 oceanDeepColor = glm::vec3(0.01f, 0.06f, 0.18f);
    PlanetRenderMode renderMode = PlanetRenderMode::Shaded;
    bool showWireOverlay = false;
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

    PlanetRenderer();

    void initialize();
    void setPlanetRotation(float yawDegrees, float pitchDegrees);

    PlanetRenderSettings& settings();
    const PlanetRenderSettings& settings() const;

    void render(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

    const char* currentModeLabel() const;
    std::size_t visiblePatchCount() const;
    const CullingStats& cullingStats() const;

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

    struct RenderPatch {
        int faceIndex = 0;
        glm::vec2 uvMin{0.0f, 0.0f};
        glm::vec2 uvSize{1.0f, 1.0f};
        int depth = 0;
    };

    struct Frustum {
        std::array<glm::vec4, 6> planes{};
    };

    struct TerrainMesh {
        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLuint indexBufferObject = 0;
        GLsizei indexCount = 0;

        void buildGrid(int patchResolution);
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
    static constexpr float kLodSplitPixels = 140.0f;

    static const std::array<FaceBasis, 6> kPlanetFaces;

    PlanetRenderSettings settings_;
    TerrainMesh terrainMesh_;
    RenderTarget reflectionTarget_;
    RenderTarget refractionTarget_;
    ShaderProgram terrainProgram_;
    ShaderProgram oceanProgram_;
    ShaderProgram wireOverlayProgram_;
    ShaderProgram coarseGridProgram_;
    glm::mat4 modelMatrix_;
    glm::vec3 lightDirection_;
    float planetYawDegrees_ = 0.0f;
    float planetPitchDegrees_ = 0.0f;
    std::vector<RenderPatch> visiblePatches_;
    CullingStats lastCullingStats_;
    bool initialized_ = false;

    static glm::vec3 cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv);
    static glm::vec3 nodeCenterDirection(const FaceBasis& face, const QuadtreeNode& node);
    static Frustum extractFrustum(const glm::mat4& viewProjectionMatrix);
    static glm::vec4 normalizePlane(const glm::vec4& plane);
    glm::vec3 worldDirection(const glm::vec3& localDirection) const;
    glm::vec3 nodeCenterWorldPosition(const FaceBasis& face, const QuadtreeNode& node, float radius) const;

    float nodeWorldRadius(const FaceBasis& face, const QuadtreeNode& node) const;
    bool isNodeOutsideFrustum(const Frustum& frustum, const FaceBasis& face, const QuadtreeNode& node) const;
    bool isNodeHiddenByHorizon(const FlyCamera& camera, const FaceBasis& face, const QuadtreeNode& node) const;
    bool shouldSplitNode(const FlyCamera& camera, const FaceBasis& face, const QuadtreeNode& node, int framebufferHeight) const;

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
    void drawReflectionRefractionPasses(const FlyCamera& camera,
                                        const glm::mat4& viewMatrix,
                                        const glm::mat4& projectionMatrix,
                                        int framebufferWidth,
                                        int framebufferHeight);
    void drawWireOverlayPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
};
