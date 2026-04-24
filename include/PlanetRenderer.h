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
    float coarseGridLineWidth = 1.6f;
    float oceanAlpha = 0.78f;
    float oceanFresnelStrength = 0.65f;
    glm::vec3 oceanShallowColor = glm::vec3(0.10f, 0.42f, 0.66f);
    glm::vec3 oceanDeepColor = glm::vec3(0.02f, 0.10f, 0.24f);
    PlanetRenderMode renderMode = PlanetRenderMode::Shaded;
    bool showWireOverlay = false;
    bool renderOcean = true;
    bool animateTerrain = false;
    float animationTime = 0.0f;
};

class PlanetRenderer
{
public:
    PlanetRenderer();

    void initialize();
    void updateAnimationTime(float seconds);

    PlanetRenderSettings& settings();
    const PlanetRenderSettings& settings() const;

    void render(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);

    const char* currentModeLabel() const;
    std::size_t visiblePatchCount() const;

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

    struct TerrainMesh {
        GLuint vertexArrayObject = 0;
        GLuint vertexBufferObject = 0;
        GLuint indexBufferObject = 0;
        GLsizei indexCount = 0;

        void buildGrid(int patchResolution);
        void draw() const;
    };

    static constexpr int kNodeGridResolution = 8;
    static constexpr int kMinimumLodDepth = 1;
    static constexpr int kMaximumLodDepth = 6;
    static constexpr float kLodSplitPixels = 140.0f;

    static const std::array<FaceBasis, 6> kPlanetFaces;

    PlanetRenderSettings settings_;
    TerrainMesh terrainMesh_;
    ShaderProgram terrainProgram_;
    ShaderProgram oceanProgram_;
    ShaderProgram wireOverlayProgram_;
    ShaderProgram coarseGridProgram_;
    glm::mat4 modelMatrix_;
    glm::vec3 lightDirection_;
    std::vector<RenderPatch> visiblePatches_;
    bool initialized_ = false;

    static glm::vec3 cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv);
    static glm::vec3 nodeCenterDirection(const FaceBasis& face, const QuadtreeNode& node);

    float nodeWorldRadius(const FaceBasis& face, const QuadtreeNode& node) const;
    bool isNodeHiddenByHorizon(const FlyCamera& camera, const FaceBasis& face, const QuadtreeNode& node) const;
    bool shouldSplitNode(const FlyCamera& camera, const FaceBasis& face, const QuadtreeNode& node, int framebufferHeight) const;

    void collectVisiblePatches(const FlyCamera& camera,
                               int faceIndex,
                               const QuadtreeNode& node,
                               int framebufferHeight,
                               std::vector<RenderPatch>& outPatches) const;

    std::vector<RenderPatch> buildVisiblePatches(const FlyCamera& camera, int framebufferHeight) const;

    void applyCommonUniforms(const ShaderProgram& program,
                             const FlyCamera& camera,
                             const glm::mat4& viewMatrix,
                             const glm::mat4& projectionMatrix,
                             const RenderPatch& patch) const;

    float seaLevelRadius() const;

    void drawTerrainPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void drawOceanPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void drawWireOverlayPass(const FlyCamera& camera, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
};
