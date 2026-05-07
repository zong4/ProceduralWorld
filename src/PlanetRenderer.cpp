#include "PlanetRenderer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

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

void PlanetRenderer::RenderTarget::release()
{
    if (depthTexture != 0) {
        glDeleteTextures(1, &depthTexture);
        depthTexture = 0;
    }
    if (colorTexture != 0) {
        glDeleteTextures(1, &colorTexture);
        colorTexture = 0;
    }
    if (framebufferObject != 0) {
        glDeleteFramebuffers(1, &framebufferObject);
        framebufferObject = 0;
    }
    width = 0;
    height = 0;
}

void PlanetRenderer::RenderTarget::create(int targetWidth, int targetHeight)
{
    if (targetWidth <= 0 || targetHeight <= 0) {
        return;
    }

    if (width == targetWidth && height == targetHeight && framebufferObject != 0) {
        return;
    }

    release();

    width = targetWidth;
    height = targetHeight;

    glGenFramebuffers(1, &framebufferObject);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferObject);

    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[PlanetRenderer] Failed to create render target framebuffer\n";
        release();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

    oceanWireOverlayProgram_ = ShaderProgram("shaders/ocean.vert",
                                             "shaders/ocean.tesc",
                                             "shaders/ocean.tese",
                                             "shaders/wire_fine.frag");

    oceanCoarseGridProgram_ = ShaderProgram("shaders/ocean.vert",
                                            "shaders/ocean.tesc",
                                            "shaders/ocean.tese",
                                            "shaders/wire_coarse.frag");

    terrainMesh_.buildGrid(kNodeGridResolution);
    fftOcean_.initialize();
    glPatchParameteri(GL_PATCH_VERTICES, 4);

    initialized_ = true;
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
                            const glm::mat4& projectionMatrix,
                            float timeSeconds)
{
    if (!initialized_) return;
    currentTimeSeconds_ = timeSeconds;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int framebufferWidth = std::max(viewport[2], 1);
    const int framebufferHeight = std::max(viewport[3], 1);

    const Frustum frustum = extractFrustum(projectionMatrix * viewMatrix);
    visiblePatches_ = buildVisiblePatches(camera, frustum, framebufferHeight);
    fftOcean_.update(timeSeconds);
    drawReflectionRefractionPasses(camera, viewMatrix, projectionMatrix, framebufferWidth, framebufferHeight);
    drawTerrainPass(camera, viewMatrix, projectionMatrix);
    drawOceanPass(camera, viewMatrix, projectionMatrix);
    drawWireOverlayPass(camera, viewMatrix, projectionMatrix);
}

const char* PlanetRenderer::currentModeLabel() const
{
    const char* wireLabel = "";
    if (settings_.wireMode == PlanetWireMode::Terrain) {
        wireLabel = "+TerrainWire";
    } else if (settings_.wireMode == PlanetWireMode::Ocean) {
        wireLabel = "+OceanWire";
    }

    switch (settings_.renderMode) {
    case PlanetRenderMode::Unshaded:
        return settings_.wireMode == PlanetWireMode::None ? "Unshaded" : wireLabel[1] == 'T' ? "Unshaded+TerrainWire" : "Unshaded+OceanWire";
    case PlanetRenderMode::HeightMap:
        return settings_.wireMode == PlanetWireMode::None ? "HeightMap" : wireLabel[1] == 'T' ? "Height+TerrainWire" : "Height+OceanWire";
    case PlanetRenderMode::Normals:
        return settings_.wireMode == PlanetWireMode::None ? "Normals" : wireLabel[1] == 'T' ? "Normals+TerrainWire" : "Normals+OceanWire";
    case PlanetRenderMode::Shaded:
    default:
        return settings_.wireMode == PlanetWireMode::None ? "Shaded" : wireLabel[1] == 'T' ? "Shaded+TerrainWire" : "Shaded+OceanWire";
    }
}

std::size_t PlanetRenderer::visiblePatchCount() const
{
    return visiblePatches_.size();
}

const PlanetRenderer::CullingStats& PlanetRenderer::cullingStats() const
{
    return lastCullingStats_;
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

PlanetRenderer::Frustum PlanetRenderer::extractFrustum(const glm::mat4& viewProjectionMatrix)
{
    const glm::vec4 row0(viewProjectionMatrix[0][0], viewProjectionMatrix[1][0], viewProjectionMatrix[2][0], viewProjectionMatrix[3][0]);
    const glm::vec4 row1(viewProjectionMatrix[0][1], viewProjectionMatrix[1][1], viewProjectionMatrix[2][1], viewProjectionMatrix[3][1]);
    const glm::vec4 row2(viewProjectionMatrix[0][2], viewProjectionMatrix[1][2], viewProjectionMatrix[2][2], viewProjectionMatrix[3][2]);
    const glm::vec4 row3(viewProjectionMatrix[0][3], viewProjectionMatrix[1][3], viewProjectionMatrix[2][3], viewProjectionMatrix[3][3]);

    Frustum frustum;
    frustum.planes[0] = normalizePlane(row3 + row0);
    frustum.planes[1] = normalizePlane(row3 - row0);
    frustum.planes[2] = normalizePlane(row3 + row1);
    frustum.planes[3] = normalizePlane(row3 - row1);
    frustum.planes[4] = normalizePlane(row3 + row2);
    frustum.planes[5] = normalizePlane(row3 - row2);
    return frustum;
}

glm::vec4 PlanetRenderer::normalizePlane(const glm::vec4& plane)
{
    const float normalLength = glm::length(glm::vec3(plane));
    if (normalLength <= 0.00001f) {
        return plane;
    }
    return plane / normalLength;
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

bool PlanetRenderer::isNodeOutsideFrustum(const Frustum& frustum,
                                          const FaceBasis& face,
                                          const QuadtreeNode& node) const
{
    const float boundsRadius = settings_.planetRadius
                             + settings_.terrainHeightScale * 2.0f
                             + glm::abs(settings_.seaLevelOffset * settings_.terrainHeightScale);
    const glm::vec3 center = nodeCenterWorldPosition(face, node, boundsRadius);
    const float radius = nodeWorldRadius(face, node) + settings_.terrainHeightScale * 2.0f;

    for (const glm::vec4& plane : frustum.planes) {
        const float signedDistance = glm::dot(glm::vec3(plane), center) + plane.w;
        if (signedDistance < -radius) {
            return true;
        }
    }

    return false;
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
                                           const Frustum& frustum,
                                           int faceIndex,
                                           const QuadtreeNode& node,
                                           int framebufferHeight,
                                           CullingStats& stats,
                                           std::vector<RenderPatch>& outPatches) const
{
    const FaceBasis& face = kPlanetFaces[faceIndex];
    ++stats.visitedNodes;

    if (isNodeOutsideFrustum(frustum, face, node)) {
        ++stats.frustumCulledNodes;
        return;
    }

    if (isNodeHiddenByHorizon(camera, face, node)) {
        ++stats.horizonCulledNodes;
        return;
    }

    if (shouldSplitNode(camera, face, node, framebufferHeight)) {
        ++stats.splitNodes;
        const float childSize = node.uvSize * 0.5f;

        for (int childY = 0; childY < 2; ++childY) {
            for (int childX = 0; childX < 2; ++childX) {
                QuadtreeNode childNode;
                childNode.uvMin = node.uvMin + glm::vec2(childX * childSize, childY * childSize);
                childNode.uvSize = childSize;
                childNode.depth = node.depth + 1;
                collectVisiblePatches(camera, frustum, faceIndex, childNode, framebufferHeight, stats, outPatches);
            }
        }
        return;
    }

    RenderPatch patch;
    patch.faceIndex = faceIndex;
    patch.uvMin = node.uvMin;
    patch.uvSize = glm::vec2(node.uvSize);
    patch.depth = node.depth;
    ++stats.emittedPatches;
    outPatches.push_back(patch);
}

std::vector<PlanetRenderer::RenderPatch> PlanetRenderer::buildVisiblePatches(const FlyCamera& camera,
                                                                             const Frustum& frustum,
                                                                             int framebufferHeight)
{
    std::vector<RenderPatch> patches;
    patches.reserve(256);
    CullingStats stats;

    const QuadtreeNode rootNode;
    for (int faceIndex = 0; faceIndex < static_cast<int>(kPlanetFaces.size()); ++faceIndex) {
        collectVisiblePatches(camera, frustum, faceIndex, rootNode, framebufferHeight, stats, patches);
    }

    lastCullingStats_ = stats;
    return patches;
}

void PlanetRenderer::applyCommonUniforms(const ShaderProgram& program,
                                         const FlyCamera& camera,
                                         const glm::mat4& viewMatrix,
                                         const glm::mat4& projectionMatrix,
                                         const RenderPatch& patch) const
{
    const FaceBasis& face = kPlanetFaces[patch.faceIndex];
    const glm::mat4 cameraRelativeView = glm::mat4(glm::mat3(viewMatrix));
    const float cameraAltitude = glm::max(glm::length(camera.position) - settings_.planetRadius, 0.0f);

    program.use();
    program.setMat4("model", modelMatrix_);
    program.setMat4("view", viewMatrix);
    program.setMat4("cameraRelativeView", cameraRelativeView);
    program.setMat4("projection", projectionMatrix);
    program.setVec3("cameraPos", camera.position);
    program.setVec3("lightDir", lightDirection_);
    program.setFloat("cameraAltitude", cameraAltitude);
    program.setFloat("tessMin", settings_.tessellationMin);
    program.setFloat("tessMax", settings_.tessellationMax);
    program.setFloat("tessMinDist", settings_.tessellationNearDistance);
    program.setFloat("tessMaxDist", settings_.tessellationFarDistance);
    program.setFloat("planetRadius", settings_.planetRadius);
    program.setFloat("seaLevelRadius", seaLevelRadius());
    program.setFloat("heightScale", settings_.terrainHeightScale);
    program.setFloat("noiseScale", settings_.terrainNoiseScale);
    program.setFloat("regionalDetailStrength", settings_.regionalDetailStrength);
    program.setFloat("microDetailStrength", settings_.microDetailStrength);
    program.setFloat("regionalDetailStartAltitude", settings_.regionalDetailStartAltitude);
    program.setFloat("regionalDetailEndAltitude", settings_.regionalDetailEndAltitude);
    program.setFloat("microDetailStartAltitude", settings_.microDetailStartAltitude);
    program.setFloat("microDetailEndAltitude", settings_.microDetailEndAltitude);
    program.setFloat("timeSeconds", currentTimeSeconds_);
    program.setFloat("gridCount", static_cast<float>(kNodeGridResolution));
    program.setFloat("coarseLineWidth", settings_.coarseGridLineWidth);
    program.setFloat("oceanAlpha", settings_.oceanAlpha);
    program.setFloat("oceanFresnelStrength", settings_.oceanFresnelStrength);
    program.setFloat("oceanDistortionStrength", settings_.oceanDistortionStrength);
    program.setFloat("oceanDepthRange", settings_.oceanDepthRange);
    program.setFloat("oceanWaveAmplitude", settings_.oceanWaveAmplitude);
    program.setFloat("oceanChoppiness", settings_.oceanChoppiness);
    program.setFloat("oceanWaveTileScale", settings_.oceanWaveTileScale);
    program.setFloat("oceanHeightTexelSize", fftOcean_.texelSize());
    program.setFloat("oceanWaveNormalStrength", settings_.oceanWaveNormalStrength);
    program.setFloat("oceanDetailNormalStrength", settings_.oceanDetailNormalStrength);
    program.setFloat("oceanDetailNormalScale", settings_.oceanDetailNormalScale);
    program.setFloat("oceanDetailFadeDistance", settings_.oceanDetailFadeDistance);
    program.setFloat("oceanSpecularStrength", settings_.oceanSpecularStrength);
    program.setFloat("oceanSpecularSharpness", settings_.oceanSpecularSharpness);
    program.setFloat("oceanFoamAmount", settings_.oceanFoamAmount);
    program.setFloat("oceanFoamThreshold", settings_.oceanFoamThreshold);
    program.setFloat("oceanFoamSoftness", settings_.oceanFoamSoftness);
    program.setFloat("oceanFoamScale", settings_.oceanFoamScale);
    program.setFloat("oceanFoamNoiseStrength", settings_.oceanFoamNoiseStrength);
    program.setFloat("oceanFoamCrestPower", settings_.oceanFoamCrestPower);
    program.setFloat("oceanFoamSlopeWeight", settings_.oceanFoamSlopeWeight);
    program.setFloat("oceanFoamFoldWeight", settings_.oceanFoamFoldWeight);
    program.setFloat("oceanFoamFadeDistance", settings_.oceanFoamFadeDistance);
    program.setVec2("oceanWindDirection", fftOcean_.settings().windDirection);
    program.setInt("renderMode", static_cast<int>(settings_.renderMode));
    program.setVec2("nodeUvMin", patch.uvMin);
    program.setVec2("nodeUvSize", patch.uvSize);
    program.setVec3("oceanShallowColor", settings_.oceanShallowColor);
    program.setVec3("oceanDeepColor", settings_.oceanDeepColor);
    program.setVec3("oceanFoamColor", settings_.oceanFoamColor);
    program.setVec3("faceNormal", face.normal);
    program.setVec3("faceAxisU", face.axisU);
    program.setVec3("faceAxisV", face.axisV);
}

void PlanetRenderer::drawTerrainPass(const FlyCamera& camera,
                                     const glm::mat4& viewMatrix,
                                     const glm::mat4& projectionMatrix)
{
    drawTerrainPass(camera, viewMatrix, projectionMatrix, false, 0.0f, true);
}

void PlanetRenderer::drawTerrainPass(const FlyCamera& camera,
                                     const glm::mat4& viewMatrix,
                                     const glm::mat4& projectionMatrix,
                                     bool useClipPlane,
                                     float clipPlaneY,
                                     bool keepAboveClipPlane)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (useClipPlane) {
        glEnable(GL_CLIP_DISTANCE0);
    } else {
        glDisable(GL_CLIP_DISTANCE0);
    }

    for (const RenderPatch& patch : visiblePatches_) {
        applyCommonUniforms(terrainProgram_, camera, viewMatrix, projectionMatrix, patch);
        terrainProgram_.setVec4("clipPlane", useClipPlane ? glm::vec4(0.0f, keepAboveClipPlane ? 1.0f : -1.0f, 0.0f, -clipPlaneY)
                                                         : glm::vec4(0.0f, 0.0f, 0.0f, -1.0e9f));
        terrainMesh_.draw();
    }

    glDisable(GL_CLIP_DISTANCE0);
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

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, reflectionTarget_.colorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, refractionTarget_.colorTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, refractionTarget_.depthTexture);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.heightTexture());
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.normalTexture());
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.detailNormalTextureA());
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.detailNormalTextureB());
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.foamNoiseTexture());
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.displacementTexture());
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.foldingTexture());

    for (const RenderPatch& patch : visiblePatches_) {
        applyCommonUniforms(oceanProgram_, camera, viewMatrix, projectionMatrix, patch);
        oceanProgram_.setFloat("tessMin", settings_.oceanTessellationMin);
        oceanProgram_.setFloat("tessMax", settings_.oceanTessellationMax);
        oceanProgram_.setFloat("tessMinDist", settings_.oceanTessellationNearDistance);
        oceanProgram_.setFloat("tessMaxDist", settings_.oceanTessellationFarDistance);
        oceanProgram_.setInt("reflectionTexture", 0);
        oceanProgram_.setInt("refractionTexture", 1);
        oceanProgram_.setInt("refractionDepthTexture", 2);
        oceanProgram_.setInt("oceanHeightTexture", 3);
        oceanProgram_.setInt("oceanNormalTexture", 4);
        oceanProgram_.setInt("waterDetailNormalTextureA", 5);
        oceanProgram_.setInt("waterDetailNormalTextureB", 6);
        oceanProgram_.setInt("foamNoiseTexture", 7);
        oceanProgram_.setInt("oceanDisplacementTexture", 8);
        oceanProgram_.setInt("oceanFoldingTexture", 9);
        terrainMesh_.draw();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void PlanetRenderer::drawReflectionRefractionPasses(const FlyCamera& camera,
                                                    const glm::mat4& viewMatrix,
                                                    const glm::mat4& projectionMatrix,
                                                    int framebufferWidth,
                                                    int framebufferHeight)
{
    if (!settings_.renderOcean) {
        return;
    }

    reflectionTarget_.create(framebufferWidth, framebufferHeight);
    refractionTarget_.create(framebufferWidth, framebufferHeight);
    if (reflectionTarget_.framebufferObject == 0 || refractionTarget_.framebufferObject == 0) {
        return;
    }

    const float seaLevelY = seaLevelRadius();
    const glm::vec3 normal(0.0f, 1.0f, 0.0f);
    const float reflectionPlaneY = seaLevelY;
    const float refractionPlaneY = seaLevelY;

    glBindFramebuffer(GL_FRAMEBUFFER, reflectionTarget_.framebufferObject);
    glViewport(0, 0, reflectionTarget_.width, reflectionTarget_.height);
    glClearColor(0.53f, 0.73f, 0.94f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    FlyCamera reflectedCamera = camera;
    reflectedCamera.position = glm::vec3(
        camera.position.x,
        2.0f * seaLevelY - camera.position.y,
        camera.position.z
    );
    reflectedCamera.front = glm::normalize(glm::vec3(
        camera.front.x,
        -camera.front.y,
        camera.front.z
    ));
    reflectedCamera.up = glm::normalize(glm::vec3(
        camera.up.x,
        -camera.up.y,
        camera.up.z
    ));
    reflectedCamera.right = glm::normalize(glm::cross(reflectedCamera.front, reflectedCamera.worldUp));
    const glm::mat4 reflectionView = glm::lookAt(
        reflectedCamera.position,
        reflectedCamera.position + reflectedCamera.front,
        reflectedCamera.up
    );
    drawTerrainPass(reflectedCamera, reflectionView, projectionMatrix, true, reflectionPlaneY, false);

    glBindFramebuffer(GL_FRAMEBUFFER, refractionTarget_.framebufferObject);
    glViewport(0, 0, refractionTarget_.width, refractionTarget_.height);
    glClearColor(0.53f, 0.73f, 0.94f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawTerrainPass(camera, viewMatrix, projectionMatrix, true, refractionPlaneY, true);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

void PlanetRenderer::drawWireOverlayPass(const FlyCamera& camera,
                                         const glm::mat4& viewMatrix,
                                         const glm::mat4& projectionMatrix)
{
    if (settings_.wireMode == PlanetWireMode::None) {
        return;
    }

    const bool drawOceanWire = settings_.wireMode == PlanetWireMode::Ocean;
    if (drawOceanWire && !settings_.renderOcean) {
        return;
    }

    ShaderProgram& fineProgram = drawOceanWire ? oceanWireOverlayProgram_ : wireOverlayProgram_;
    ShaderProgram& coarseProgram = drawOceanWire ? oceanCoarseGridProgram_ : coarseGridProgram_;

    if (drawOceanWire) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, fftOcean_.heightTexture());
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, fftOcean_.normalTexture());
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, fftOcean_.displacementTexture());
    }

    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    for (const RenderPatch& patch : visiblePatches_) {
        applyCommonUniforms(fineProgram, camera, viewMatrix, projectionMatrix, patch);
        if (drawOceanWire) {
            fineProgram.setFloat("tessMin", settings_.oceanTessellationMin);
            fineProgram.setFloat("tessMax", settings_.oceanTessellationMax);
            fineProgram.setFloat("tessMinDist", settings_.oceanTessellationNearDistance);
            fineProgram.setFloat("tessMaxDist", settings_.oceanTessellationFarDistance);
            fineProgram.setInt("oceanHeightTexture", 3);
            fineProgram.setInt("oceanNormalTexture", 4);
            fineProgram.setInt("oceanDisplacementTexture", 8);
        }
        terrainMesh_.draw();
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    for (const RenderPatch& patch : visiblePatches_) {
        applyCommonUniforms(coarseProgram, camera, viewMatrix, projectionMatrix, patch);
        if (drawOceanWire) {
            coarseProgram.setFloat("tessMin", settings_.oceanTessellationMin);
            coarseProgram.setFloat("tessMax", settings_.oceanTessellationMax);
            coarseProgram.setFloat("tessMinDist", settings_.oceanTessellationNearDistance);
            coarseProgram.setFloat("tessMaxDist", settings_.oceanTessellationFarDistance);
            coarseProgram.setInt("oceanHeightTexture", 3);
            coarseProgram.setInt("oceanNormalTexture", 4);
            coarseProgram.setInt("oceanDisplacementTexture", 8);
        }
        terrainMesh_.draw();
    }

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}
