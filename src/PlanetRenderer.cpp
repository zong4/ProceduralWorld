#include "PlanetRenderer.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

const std::array<PlanetRenderer::FaceBasis, 6> PlanetRenderer::kPlanetFaces = {{
    {{ 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f, -1.0f}, { 0.0f,  1.0f,  0.0f}},
    {{-1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f,  1.0f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.0f,  1.0f,  0.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f, -1.0f}},
    {{ 0.0f, -1.0f,  0.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.0f,  0.0f,  1.0f}, { 1.0f,  0.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.0f,  0.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}}
}};

PlanetRenderer::PlanetRenderer()
    : modelMatrix_(1.0f),
      lightDirection_(glm::normalize(glm::vec3(-0.6f, -1.0f, -0.4f)))
{
}

void PlanetRenderer::initialize()
{
    if (initialized_) return;

    terrainProgram_ = ShaderProgram("shaders/terrain.vert",
                                    "shaders/terrain.tesc",
                                    "shaders/terrain.tese",
                                    "shaders/terrain.frag");

    oceanProgram_ = ShaderProgram("shaders/ocean.vert",
                                  "shaders/ocean.tesc",
                                  "shaders/ocean.tese",
                                  "shaders/ocean.frag");

    wireOverlayProgram_ = ShaderProgram("shaders/terrain.vert",
                                        "shaders/terrain.tesc",
                                        "shaders/terrain.tese",
                                        "shaders/wire_fine.frag");

    coarseGridProgram_ = ShaderProgram("shaders/terrain.vert",
                                       "shaders/terrain.tesc",
                                       "shaders/terrain.tese",
                                       "shaders/wire_coarse.frag");

    terrainMesh_.buildGrid(kNodeGridResolution);
    glPatchParameteri(GL_PATCH_VERTICES, 4);

    initialized_ = true;
}

void PlanetRenderer::updateAnimationTime(float seconds)
{
    settings_.animationTime = seconds;
}

void PlanetRenderer::setPlanetRotation(float yawDegrees, float pitchDegrees)
{
    planetYawDegrees_ = yawDegrees;
    planetPitchDegrees_ = pitchDegrees;

    glm::mat4 rotationMatrix(1.0f);
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(planetYawDegrees_), glm::vec3(0.0f, 1.0f, 0.0f));
    rotationMatrix = glm::rotate(rotationMatrix, glm::radians(planetPitchDegrees_), glm::vec3(1.0f, 0.0f, 0.0f));
    modelMatrix_ = rotationMatrix;
}

PlanetRenderSettings& PlanetRenderer::settings()
{
    return settings_;
}

const PlanetRenderSettings& PlanetRenderer::settings() const
{
    return settings_;
}

void PlanetRenderer::render(const FlyCamera& camera,
                            const glm::mat4& viewMatrix,
                            const glm::mat4& projectionMatrix)
{
    if (!initialized_) return;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int framebufferHeight = std::max(viewport[3], 1);

    visiblePatches_ = buildVisiblePatches(camera, framebufferHeight);
    drawTerrainPass(camera, viewMatrix, projectionMatrix);
    drawOceanPass(camera, viewMatrix, projectionMatrix);
    drawWireOverlayPass(camera, viewMatrix, projectionMatrix);
}

const char* PlanetRenderer::currentModeLabel() const
{
    switch (settings_.renderMode) {
    case PlanetRenderMode::HeightMap:
        return settings_.showWireOverlay ? "Height+Wire" : "HeightMap";
    case PlanetRenderMode::Normals:
        return settings_.showWireOverlay ? "Normals+Wire" : "Normals";
    case PlanetRenderMode::Shaded:
    default:
        return settings_.showWireOverlay ? "Shaded+Wire" : "Shaded";
    }
}

std::size_t PlanetRenderer::visiblePatchCount() const
{
    return visiblePatches_.size();
}

float PlanetRenderer::seaLevelRadius() const
{
    return settings_.planetRadius + settings_.seaLevelOffset * settings_.terrainHeightScale;
}

void PlanetRenderer::TerrainMesh::buildGrid(int patchResolution)
{
    std::vector<float> vertices;
    std::vector<unsigned> indices;

    const float uvStep = 1.0f / static_cast<float>(patchResolution);

    for (int row = 0; row <= patchResolution; ++row) {
        for (int column = 0; column <= patchResolution; ++column) {
            vertices.push_back(column * uvStep);
            vertices.push_back(row * uvStep);
        }
    }

    for (int row = 0; row < patchResolution; ++row) {
        for (int column = 0; column < patchResolution; ++column) {
            const unsigned bottomLeft = row * (patchResolution + 1) + column;
            const unsigned bottomRight = bottomLeft + 1;
            const unsigned topLeft = (row + 1) * (patchResolution + 1) + column;
            const unsigned topRight = topLeft + 1;

            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
            indices.push_back(topRight);
            indices.push_back(topLeft);
        }
    }

    indexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &vertexArrayObject);
    glGenBuffers(1, &vertexBufferObject);
    glGenBuffers(1, &indexBufferObject);

    glBindVertexArray(vertexArrayObject);

    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void PlanetRenderer::TerrainMesh::draw() const
{
    glBindVertexArray(vertexArrayObject);
    glDrawElements(GL_PATCHES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

glm::vec3 PlanetRenderer::cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv)
{
    const glm::vec2 faceUv = uv * 2.0f - 1.0f;
    return glm::normalize(face.normal + faceUv.x * face.axisU + faceUv.y * face.axisV);
}

glm::vec3 PlanetRenderer::nodeCenterDirection(const FaceBasis& face, const QuadtreeNode& node)
{
    return cubeSphereDirection(face, node.uvMin + glm::vec2(node.uvSize * 0.5f));
}

glm::vec3 PlanetRenderer::worldDirection(const glm::vec3& localDirection) const
{
    return glm::normalize(glm::mat3(modelMatrix_) * localDirection);
}

glm::vec3 PlanetRenderer::nodeCenterWorldPosition(const FaceBasis& face, const QuadtreeNode& node, float radius) const
{
    return worldDirection(nodeCenterDirection(face, node)) * radius;
}

float PlanetRenderer::nodeWorldRadius(const FaceBasis& face, const QuadtreeNode& node) const
{
    const float sampleRadius = settings_.planetRadius + settings_.terrainHeightScale * 2.0f;
    const glm::vec3 center = nodeCenterWorldPosition(face, node, sampleRadius);

    const glm::vec2 uv00 = node.uvMin;
    const glm::vec2 uv10 = node.uvMin + glm::vec2(node.uvSize, 0.0f);
    const glm::vec2 uv01 = node.uvMin + glm::vec2(0.0f, node.uvSize);
    const glm::vec2 uv11 = node.uvMin + glm::vec2(node.uvSize, node.uvSize);

    float patchRadius = 0.0f;
    patchRadius = glm::max(patchRadius, glm::length(worldDirection(cubeSphereDirection(face, uv00)) * sampleRadius - center));
    patchRadius = glm::max(patchRadius, glm::length(worldDirection(cubeSphereDirection(face, uv10)) * sampleRadius - center));
    patchRadius = glm::max(patchRadius, glm::length(worldDirection(cubeSphereDirection(face, uv01)) * sampleRadius - center));
    patchRadius = glm::max(patchRadius, glm::length(worldDirection(cubeSphereDirection(face, uv11)) * sampleRadius - center));
    return glm::max(patchRadius, 0.001f);
}

bool PlanetRenderer::isNodeHiddenByHorizon(const FlyCamera& camera,
                                           const FaceBasis& face,
                                           const QuadtreeNode& node) const
{
    const float cameraDistanceFromOrigin = glm::length(camera.position);
    if (cameraDistanceFromOrigin <= settings_.planetRadius + settings_.terrainHeightScale * 2.0f) {
        return false;
    }

    const glm::vec3 cameraDirection = glm::normalize(camera.position);
    const glm::vec3 nodeDirection = worldDirection(nodeCenterDirection(face, node));
    const float nodeRadius = nodeWorldRadius(face, node);
    const float horizonDot = settings_.planetRadius / cameraDistanceFromOrigin;
    const float safetyMargin = nodeRadius / settings_.planetRadius;
    return glm::dot(cameraDirection, nodeDirection) < horizonDot - safetyMargin;
}

bool PlanetRenderer::shouldSplitNode(const FlyCamera& camera,
                                     const FaceBasis& face,
                                     const QuadtreeNode& node,
                                     int framebufferHeight) const
{
    if (node.depth < kMinimumLodDepth) return true;
    if (node.depth >= kMaximumLodDepth) return false;

    const glm::vec3 patchCenter = nodeCenterWorldPosition(face, node, settings_.planetRadius);
    const float distanceToCamera = glm::length(camera.position - patchCenter);
    const float patchRadius = nodeWorldRadius(face, node);
    const float projectionScale = (0.5f * static_cast<float>(framebufferHeight))
                                / glm::tan(glm::radians(camera.fieldOfView) * 0.5f);
    const float projectedRadius = patchRadius * projectionScale / glm::max(distanceToCamera, 0.001f);
    return projectedRadius > kLodSplitPixels;
}

void PlanetRenderer::collectVisiblePatches(const FlyCamera& camera,
                                           int faceIndex,
                                           const QuadtreeNode& node,
                                           int framebufferHeight,
                                           std::vector<RenderPatch>& outPatches) const
{
    const FaceBasis& face = kPlanetFaces[faceIndex];

    if (isNodeHiddenByHorizon(camera, face, node)) {
        return;
    }

    if (shouldSplitNode(camera, face, node, framebufferHeight)) {
        const float childSize = node.uvSize * 0.5f;

        for (int childY = 0; childY < 2; ++childY) {
            for (int childX = 0; childX < 2; ++childX) {
                QuadtreeNode childNode;
                childNode.uvMin = node.uvMin + glm::vec2(childX * childSize, childY * childSize);
                childNode.uvSize = childSize;
                childNode.depth = node.depth + 1;
                collectVisiblePatches(camera, faceIndex, childNode, framebufferHeight, outPatches);
            }
        }
        return;
    }

    RenderPatch patch;
    patch.faceIndex = faceIndex;
    patch.uvMin = node.uvMin;
    patch.uvSize = glm::vec2(node.uvSize);
    patch.depth = node.depth;
    outPatches.push_back(patch);
}

std::vector<PlanetRenderer::RenderPatch> PlanetRenderer::buildVisiblePatches(const FlyCamera& camera,
                                                                             int framebufferHeight) const
{
    std::vector<RenderPatch> patches;
    patches.reserve(256);

    const QuadtreeNode rootNode;
    for (int faceIndex = 0; faceIndex < static_cast<int>(kPlanetFaces.size()); ++faceIndex) {
        collectVisiblePatches(camera, faceIndex, rootNode, framebufferHeight, patches);
    }

    return patches;
}

void PlanetRenderer::applyCommonUniforms(const ShaderProgram& program,
                                         const FlyCamera& camera,
                                         const glm::mat4& viewMatrix,
                                         const glm::mat4& projectionMatrix,
                                         const RenderPatch& patch) const
{
    const FaceBasis& face = kPlanetFaces[patch.faceIndex];

    program.use();
    program.setMat4("model", modelMatrix_);
    program.setMat4("view", viewMatrix);
    program.setMat4("projection", projectionMatrix);
    program.setVec3("cameraPos", camera.position);
    program.setVec3("lightDir", lightDirection_);
    program.setFloat("time", settings_.animationTime);
    program.setFloat("tessMin", settings_.tessellationMin);
    program.setFloat("tessMax", settings_.tessellationMax);
    program.setFloat("tessMinDist", settings_.tessellationNearDistance);
    program.setFloat("tessMaxDist", settings_.tessellationFarDistance);
    program.setFloat("planetRadius", settings_.planetRadius);
    program.setFloat("seaLevelRadius", seaLevelRadius());
    program.setFloat("heightScale", settings_.terrainHeightScale);
    program.setFloat("noiseScale", settings_.terrainNoiseScale);
    program.setFloat("gridCount", static_cast<float>(kNodeGridResolution));
    program.setFloat("coarseLineWidth", settings_.coarseGridLineWidth);
    program.setFloat("oceanAlpha", settings_.oceanAlpha);
    program.setFloat("oceanFresnelStrength", settings_.oceanFresnelStrength);
    program.setInt("renderMode", static_cast<int>(settings_.renderMode));
    program.setVec2("nodeUvMin", patch.uvMin);
    program.setVec2("nodeUvSize", patch.uvSize);
    program.setVec3("oceanShallowColor", settings_.oceanShallowColor);
    program.setVec3("oceanDeepColor", settings_.oceanDeepColor);
    program.setVec3("faceNormal", face.normal);
    program.setVec3("faceAxisU", face.axisU);
    program.setVec3("faceAxisV", face.axisV);
}

void PlanetRenderer::drawTerrainPass(const FlyCamera& camera,
                                     const glm::mat4& viewMatrix,
                                     const glm::mat4& projectionMatrix)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    for (const RenderPatch& patch : visiblePatches_) {
        applyCommonUniforms(terrainProgram_, camera, viewMatrix, projectionMatrix, patch);
        terrainMesh_.draw();
    }
}

void PlanetRenderer::drawOceanPass(const FlyCamera& camera,
                                   const glm::mat4& viewMatrix,
                                   const glm::mat4& projectionMatrix)
{
    if (!settings_.renderOcean) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    for (const RenderPatch& patch : visiblePatches_) {
        applyCommonUniforms(oceanProgram_, camera, viewMatrix, projectionMatrix, patch);
        terrainMesh_.draw();
    }

    glDepthMask(GL_TRUE);
}

void PlanetRenderer::drawWireOverlayPass(const FlyCamera& camera,
                                         const glm::mat4& viewMatrix,
                                         const glm::mat4& projectionMatrix)
{
    if (!settings_.showWireOverlay) {
        return;
    }

    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    for (const RenderPatch& patch : visiblePatches_) {
        applyCommonUniforms(wireOverlayProgram_, camera, viewMatrix, projectionMatrix, patch);
        terrainMesh_.draw();
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    for (const RenderPatch& patch : visiblePatches_) {
        applyCommonUniforms(coarseGridProgram_, camera, viewMatrix, projectionMatrix, patch);
        terrainMesh_.draw();
    }

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}
