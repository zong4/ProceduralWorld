#include "PlanetRenderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

#include "Instumentor/InstrumentationTimer.hpp"
#include "PlanetProceduralData.h"

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
    PROFILE_SCOPE("RenderTarget Create");
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

void PlanetRenderer::AtmosphereLut::release()
{
    if (deltaScatteringTextureB != 0) {
        glDeleteTextures(1, &deltaScatteringTextureB);
        deltaScatteringTextureB = 0;
    }
    if (deltaScatteringTextureA != 0) {
        glDeleteTextures(1, &deltaScatteringTextureA);
        deltaScatteringTextureA = 0;
    }
    if (scatteringTexture != 0) {
        glDeleteTextures(1, &scatteringTexture);
        scatteringTexture = 0;
    }
    if (irradianceTexture != 0) {
        glDeleteTextures(1, &irradianceTexture);
        irradianceTexture = 0;
    }
    if (transmittanceTexture != 0) {
        glDeleteTextures(1, &transmittanceTexture);
        transmittanceTexture = 0;
    }
    if (framebufferObject != 0) {
        glDeleteFramebuffers(1, &framebufferObject);
        framebufferObject = 0;
    }
    cachedSignature = 0;
}

void PlanetRenderer::AtmosphereLut::create()
{
    scatteringWidth = scatteringViewMuSize * scatteringNuSize;
    if (framebufferObject != 0
        && transmittanceTexture != 0
        && irradianceTexture != 0
        && scatteringTexture != 0
        && deltaScatteringTextureA != 0
        && deltaScatteringTextureB != 0) {
        return;
    }

    release();
    glGenFramebuffers(1, &framebufferObject);

    glGenTextures(1, &transmittanceTexture);
    glBindTexture(GL_TEXTURE_2D, transmittanceTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, transmittanceWidth, transmittanceHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &irradianceTexture);
    glBindTexture(GL_TEXTURE_2D, irradianceTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, irradianceWidth, irradianceHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &scatteringTexture);
    glBindTexture(GL_TEXTURE_3D, scatteringTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, scatteringWidth, scatteringHeight, scatteringDepth, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &deltaScatteringTextureA);
    glBindTexture(GL_TEXTURE_3D, deltaScatteringTextureA);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, scatteringWidth, scatteringHeight, scatteringDepth, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &deltaScatteringTextureB);
    glBindTexture(GL_TEXTURE_3D, deltaScatteringTextureB);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, scatteringWidth, scatteringHeight, scatteringDepth, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
}

void PlanetRenderer::initialize()
{
    PROFILE_SCOPE("PlanetRenderer Initialize");
    if (initialized_) return;

    terrainChunkProgram_ = ShaderProgram("shaders/terrain_chunk.vert",
                                         "shaders/terrain_chunk.frag");

    bakedChunkBoundsProgram_ = ShaderProgram("shaders/baked_chunk_bounds.vert",
                                             "shaders/baked_chunk_bounds.frag");

    featureSegmentProgram_ = ShaderProgram("shaders/feature_segments.vert",
                                           "shaders/feature_segments.frag");

    oceanProgram_ = ShaderProgram("shaders/ocean.vert",
                                  "shaders/ocean.tesc",
                                  "shaders/ocean.tese",
                                  "shaders/ocean.frag");

    oceanWireOverlayProgram_ = ShaderProgram("shaders/ocean.vert",
                                             "shaders/ocean.tesc",
                                             "shaders/ocean.tese",
                                             "shaders/ocean_wire_fine.frag");

    oceanCoarseGridProgram_ = ShaderProgram("shaders/ocean.vert",
                                            "shaders/ocean.tesc",
                                            "shaders/ocean.tese",
                                            "shaders/ocean_wire_coarse.frag");

    atmosphereProgram_ = ShaderProgram("shaders/fullscreen_triangle.vert",
                                       "shaders/atmosphere.frag");
    atmosphereTransmittanceProgram_ = ShaderProgram("shaders/fullscreen_triangle.vert",
                                                    "shaders/atmosphere_transmittance.frag");
    atmosphereIrradianceProgram_ = ShaderProgram("shaders/fullscreen_triangle.vert",
                                                 "shaders/atmosphere_irradiance.frag");
    atmosphereScatteringProgram_ = ShaderProgram("shaders/fullscreen_triangle.vert",
                                                 "shaders/atmosphere_scattering.frag");
    atmosphereAccumulateProgram_ = ShaderProgram("shaders/fullscreen_triangle.vert",
                                                 "shaders/atmosphere_accumulate.frag");

    terrainMesh_.buildGrid(kNodeGridResolution);
    atmosphereMesh_.buildSphere(96, 48);
    atmosphereLut_.create();
    glGenVertexArrays(1, &fullscreenVertexArrayObject_);
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

void PlanetRenderer::setProceduralData(const PlanetProceduralData& proceduralData)
{
    PROFILE_SCOPE("Upload Procedural Data");
    if (!proceduralData.isGenerated() || proceduralData.resolution() <= 0) {
        hasProceduralOceanData_ = false;
        proceduralWaterCoveragePrefixCpu_.clear();
        proceduralShoreCoverageLoosePrefixCpu_.clear();
        proceduralShoreCoverageStrictPrefixCpu_.clear();
        bakedTerrainMesh_.release();
        featureSegmentMesh_.release();
        return;
    }

    const int resolution = proceduralData.resolution();
    const std::size_t layerSize = static_cast<std::size_t>(resolution * resolution);
    std::vector<float> height(layerSize * 6, 0.0f);
    std::vector<float> waterDepth(layerSize * 6, 0.0f);
    std::vector<float> shoreMask(layerSize * 6, 0.0f);
    std::vector<glm::vec4> erosionData(layerSize * 6, glm::vec4(0.0f));
    std::vector<float> temperature(layerSize * 6, 0.0f);
    std::vector<float> moisture(layerSize * 6, 0.0f);
    std::vector<glm::vec4> biomeWeightA(layerSize * 6, glm::vec4(0.0f));
    std::vector<glm::vec4> biomeWeightB(layerSize * 6, glm::vec4(0.0f));
    std::vector<glm::vec4> domainWeight(layerSize * 6, glm::vec4(0.0f));

    const auto& faces = proceduralData.faces();
    for (std::size_t faceIndex = 0; faceIndex < faces.size(); ++faceIndex) {
        const auto& face = faces[faceIndex];
        if (face.height.size() != layerSize
            || face.waterDepth.size() != layerSize
            || face.shoreMask.size() != layerSize
            || face.erosionMask.size() != layerSize
            || face.channelMask.size() != layerSize
            || face.flowMask.size() != layerSize
            || face.wearMask.size() != layerSize
            || face.depositionMask.size() != layerSize
            || face.temperature.size() != layerSize
            || face.moisture.size() != layerSize
            || face.regionId.size() != layerSize
            || face.featureMask.size() != layerSize
            || face.meshDensity.size() != layerSize
            || face.geometricError.size() != layerSize
            || face.biomeWeightA.size() != layerSize
            || face.biomeWeightB.size() != layerSize
            || face.domainWeight.size() != layerSize) {
            hasProceduralOceanData_ = false;
            proceduralWaterCoveragePrefixCpu_.clear();
            proceduralShoreCoverageLoosePrefixCpu_.clear();
            proceduralShoreCoverageStrictPrefixCpu_.clear();
            bakedTerrainMesh_.release();
            featureSegmentMesh_.release();
            return;
        }

        std::copy(face.height.begin(), face.height.end(), height.begin() + static_cast<std::ptrdiff_t>(layerSize * faceIndex));
        std::copy(face.waterDepth.begin(), face.waterDepth.end(), waterDepth.begin() + static_cast<std::ptrdiff_t>(layerSize * faceIndex));
        std::copy(face.shoreMask.begin(), face.shoreMask.end(), shoreMask.begin() + static_cast<std::ptrdiff_t>(layerSize * faceIndex));
        std::copy(face.temperature.begin(), face.temperature.end(), temperature.begin() + static_cast<std::ptrdiff_t>(layerSize * faceIndex));
        std::copy(face.moisture.begin(), face.moisture.end(), moisture.begin() + static_cast<std::ptrdiff_t>(layerSize * faceIndex));
        std::copy(face.biomeWeightA.begin(), face.biomeWeightA.end(), biomeWeightA.begin() + static_cast<std::ptrdiff_t>(layerSize * faceIndex));
        std::copy(face.biomeWeightB.begin(), face.biomeWeightB.end(), biomeWeightB.begin() + static_cast<std::ptrdiff_t>(layerSize * faceIndex));
        std::copy(face.domainWeight.begin(), face.domainWeight.end(), domainWeight.begin() + static_cast<std::ptrdiff_t>(layerSize * faceIndex));

        const std::size_t layerOffset = layerSize * faceIndex;
        for (std::size_t i = 0; i < layerSize; ++i) {
            erosionData[layerOffset + i] = glm::vec4(
                face.channelMask[i],
                face.flowMask[i],
                face.wearMask[i],
                face.depositionMask[i]
            );
        }
    }

    auto uploadTextureArray = [resolution](GLuint& texture, const std::vector<float>& pixels) {
        if (texture == 0) {
            glGenTextures(1, &texture);
        }

        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            GL_R32F,
            resolution,
            resolution,
            6,
            0,
            GL_RED,
            GL_FLOAT,
            pixels.data()
        );
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    };
    auto uploadTextureArrayRgba = [resolution](GLuint& texture, const std::vector<glm::vec4>& pixels) {
        if (texture == 0) {
            glGenTextures(1, &texture);
        }

        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            GL_RGBA32F,
            resolution,
            resolution,
            6,
            0,
            GL_RGBA,
            GL_FLOAT,
            pixels.data()
        );
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    };

    uploadTextureArray(proceduralHeightTexture_, height);
    uploadTextureArray(proceduralWaterDepthTexture_, waterDepth);
    uploadTextureArrayRgba(proceduralErosionMaskTexture_, erosionData);
    uploadTextureArray(proceduralTemperatureTexture_, temperature);
    uploadTextureArray(proceduralMoistureTexture_, moisture);
    uploadTextureArrayRgba(proceduralBiomeWeightATexture_, biomeWeightA);
    uploadTextureArrayRgba(proceduralBiomeWeightBTexture_, biomeWeightB);
    uploadTextureArrayRgba(proceduralDomainWeightTexture_, domainWeight);

    const int prefixStride = resolution + 1;
    const std::size_t prefixLayerSize = static_cast<std::size_t>(prefixStride * prefixStride);
    proceduralWaterCoveragePrefixCpu_.assign(prefixLayerSize * 6, 0);
    proceduralShoreCoverageLoosePrefixCpu_.assign(prefixLayerSize * 6, 0);
    proceduralShoreCoverageStrictPrefixCpu_.assign(prefixLayerSize * 6, 0);
    constexpr float kWaterDepthEpsilon = 0.0001f;
    constexpr float kLooseShoreMaskEpsilon = 0.001f;
    constexpr float kStrictShoreMaskEpsilon = 0.015f;
    for (std::size_t faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const std::size_t sourceOffset = layerSize * faceIndex;
        const std::size_t prefixOffset = prefixLayerSize * faceIndex;
        for (int y = 0; y < resolution; ++y) {
            std::uint32_t waterRowCount = 0;
            std::uint32_t looseShoreRowCount = 0;
            std::uint32_t strictShoreRowCount = 0;
            for (int x = 0; x < resolution; ++x) {
                const std::size_t sourceIndex = sourceOffset + static_cast<std::size_t>(y * resolution + x);
                waterRowCount += waterDepth[sourceIndex] > kWaterDepthEpsilon ? 1u : 0u;
                looseShoreRowCount += shoreMask[sourceIndex] > kLooseShoreMaskEpsilon ? 1u : 0u;
                strictShoreRowCount += shoreMask[sourceIndex] > kStrictShoreMaskEpsilon ? 1u : 0u;

                const std::size_t prefixIndex = prefixOffset + static_cast<std::size_t>((y + 1) * prefixStride + (x + 1));
                const std::size_t previousRowIndex = prefixOffset + static_cast<std::size_t>(y * prefixStride + (x + 1));
                proceduralWaterCoveragePrefixCpu_[prefixIndex] =
                    proceduralWaterCoveragePrefixCpu_[previousRowIndex] + waterRowCount;
                proceduralShoreCoverageLoosePrefixCpu_[prefixIndex] =
                    proceduralShoreCoverageLoosePrefixCpu_[previousRowIndex] + looseShoreRowCount;
                proceduralShoreCoverageStrictPrefixCpu_[prefixIndex] =
                    proceduralShoreCoverageStrictPrefixCpu_[previousRowIndex] + strictShoreRowCount;
            }
        }
    }

    proceduralDataResolution_ = resolution;
    hasProceduralOceanData_ = true;
    bakedTerrainMesh_.upload(proceduralData, settings_.planetRadius, settings_.terrainHeightScale);
    featureSegmentMesh_.upload(proceduralData, settings_.planetRadius, settings_.terrainHeightScale);
}

void PlanetRenderer::render(const FlyCamera& camera,
                            const glm::mat4& viewMatrix,
                            const glm::mat4& projectionMatrix,
                            float timeSeconds)
{
    PROFILE_SCOPE("PlanetRenderer Render");
    if (!initialized_) return;
    if (hasLastRenderTimeSeconds_) {
        currentDeltaSeconds_ = glm::clamp(timeSeconds - lastRenderTimeSeconds_, 1.0f / 240.0f, 0.10f);
    } else {
        currentDeltaSeconds_ = 1.0f / 60.0f;
        hasLastRenderTimeSeconds_ = true;
    }
    lastRenderTimeSeconds_ = timeSeconds;
    currentTimeSeconds_ = timeSeconds;
    PerformanceStats frameStats;
    using Clock = std::chrono::steady_clock;
    const auto totalStart = Clock::now();
    auto elapsedMs = [](const Clock::time_point& start, const Clock::time_point& end) {
        return std::chrono::duration<float, std::milli>(end - start).count();
    };

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int framebufferWidth = std::max(viewport[2], 1);
    const int framebufferHeight = std::max(viewport[3], 1);

    auto passStart = Clock::now();
    {
        PROFILE_SCOPE("LOD and Culling");
        const Frustum frustum = extractFrustum(projectionMatrix * viewMatrix);

        CullingStats oceanCullingStats;
        if (settings_.renderOcean) {
            visibleOceanPatches_ = buildVisibleOceanPatches(camera, frustum, framebufferHeight, oceanCullingStats);
        } else {
            visibleOceanPatches_.clear();
        }

        CullingStats bakedCullingStats;
        if (settings_.renderTerrain) {
            visibleBakedChunks_ = buildVisibleBakedChunks(camera, frustum, framebufferHeight, bakedCullingStats);
        } else {
            visibleBakedChunks_.clear();
        }

        updateOceanTessellationBudget(visibleOceanPatches_.size());
        lastCullingStats_ = bakedCullingStats;
        lastCullingStats_.visitedNodes += oceanCullingStats.visitedNodes;
        lastCullingStats_.frustumCulledNodes += oceanCullingStats.frustumCulledNodes;
        lastCullingStats_.horizonCulledNodes += oceanCullingStats.horizonCulledNodes;
        lastCullingStats_.splitNodes += oceanCullingStats.splitNodes;
        lastCullingStats_.emittedPatches += oceanCullingStats.emittedPatches;
    }
    frameStats.cullingMs = elapsedMs(passStart, Clock::now());
    frameStats.oceanPatchCount = visibleOceanPatches_.size();
    frameStats.bakedChunkCount = bakedTerrainMesh_.chunks.size();
    frameStats.visibleBakedChunkCount = visibleBakedChunks_.size();
    frameStats.lodSplitPixelScale = lodSplitPixelScale_;
    frameStats.effectiveOceanTessMax = effectiveOceanTessellationMax_;
    std::size_t bakedTriangles = 0;
    for (const VisibleBakedChunk& visibleChunk : visibleBakedChunks_) {
        if (visibleChunk.chunkIndex < bakedTerrainMesh_.chunks.size()) {
            const BakedTerrainMesh::ChunkDrawRange& chunk = bakedTerrainMesh_.chunks[visibleChunk.chunkIndex];
            const std::size_t lodIndex = static_cast<std::size_t>(glm::clamp(static_cast<int>(visibleChunk.lod), 0, 2));
            bakedTriangles += chunk.lods[lodIndex].triangleCount;
            ++frameStats.visibleBakedChunkLodCount[lodIndex];
        }
    }
    frameStats.estimatedTerrainTriangles = bakedTriangles;
    frameStats.estimatedOceanTriangles = estimatePatchTriangles(visibleOceanPatches_.size(), effectiveOceanTessellationMax_);

    passStart = Clock::now();
    const int fftCascadeCount = std::clamp(settings_.oceanFftCascadeCount, 1, 3);
    const int fftFrameStride = std::max(settings_.oceanFftFrameStride, 1);
    frameStats.fftCascadeCount = fftCascadeCount;
    frameStats.fftFrameStride = fftFrameStride;
    if (settings_.renderOcean) {
        fftOcean_.setCascadeCount(fftCascadeCount);
        frameStats.fftUpdated = (oceanFftFrameCounter_ % fftFrameStride) == 0;
        if (frameStats.fftUpdated) {
            PROFILE_SCOPE("FFT Ocean Update");
            fftOcean_.update(timeSeconds);
        }
        ++oceanFftFrameCounter_;
    } else {
        oceanFftFrameCounter_ = 0;
    }
    frameStats.fftMs = elapsedMs(passStart, Clock::now());
    if (settings_.renderAtmosphere) {
        precomputeAtmosphereLuts();
    }

    passStart = Clock::now();
    drawReflectionRefractionPasses(camera, viewMatrix, projectionMatrix, framebufferWidth, framebufferHeight);
    frameStats.reflectionRefractionMs = elapsedMs(passStart, Clock::now());
    frameStats.reflectionUpdated = lastReflectionUpdated_;
    frameStats.refractionUpdated = lastRefractionUpdated_;
    frameStats.reflectionEnabled = lastReflectionEnabled_;
    frameStats.refractionEnabled = lastRefractionEnabled_;
    frameStats.reflectionFrameStride = std::max(settings_.oceanReflectionFrameStride, 1);
    frameStats.refractionFrameStride = std::max(settings_.oceanRefractionFrameStride, 1);
    frameStats.reflectionWeight = oceanReflectionWeight_;
    frameStats.refractionWeight = oceanRefractionWeight_;

    passStart = Clock::now();
    drawTerrainPass(camera, viewMatrix, projectionMatrix);
    frameStats.terrainMs = elapsedMs(passStart, Clock::now());

    passStart = Clock::now();
    drawOceanPass(camera, viewMatrix, projectionMatrix);
    frameStats.oceanMs = elapsedMs(passStart, Clock::now());

    passStart = Clock::now();
    drawAtmospherePass(camera, viewMatrix, projectionMatrix);
    frameStats.atmosphereMs = elapsedMs(passStart, Clock::now());

    passStart = Clock::now();
    drawWireOverlayPass(camera, viewMatrix, projectionMatrix);
    frameStats.wireMs = elapsedMs(passStart, Clock::now());
    frameStats.totalMs = elapsedMs(totalStart, Clock::now());
    lastPerformanceStats_ = frameStats;
}

const char* PlanetRenderer::currentModeLabel() const
{
    switch (settings_.renderMode) {
    case PlanetRenderMode::Unshaded:
        return settings_.wireMode == PlanetWireMode::None ? "Unshaded" : settings_.wireMode == PlanetWireMode::Ocean ? "Unshaded+WaterMesh" : settings_.wireMode == PlanetWireMode::MountainMask ? "Unshaded+MountainMask" : "Unshaded+BakedLOD";
    case PlanetRenderMode::HeightMap:
        return settings_.wireMode == PlanetWireMode::None ? "HeightMap" : settings_.wireMode == PlanetWireMode::Ocean ? "Height+WaterMesh" : settings_.wireMode == PlanetWireMode::MountainMask ? "Height+MountainMask" : "Height+BakedLOD";
    case PlanetRenderMode::Normals:
        return settings_.wireMode == PlanetWireMode::None ? "Normals" : settings_.wireMode == PlanetWireMode::Ocean ? "Normals+WaterMesh" : settings_.wireMode == PlanetWireMode::MountainMask ? "Normals+MountainMask" : "Normals+BakedLOD";
    case PlanetRenderMode::Material:
        return settings_.wireMode == PlanetWireMode::None ? "Material" : settings_.wireMode == PlanetWireMode::Ocean ? "Material+WaterMesh" : settings_.wireMode == PlanetWireMode::MountainMask ? "Material+MountainMask" : "Material+BakedLOD";
    case PlanetRenderMode::Shaded:
    default:
        return settings_.wireMode == PlanetWireMode::None ? "Shaded" : settings_.wireMode == PlanetWireMode::Ocean ? "Shaded+WaterMesh" : settings_.wireMode == PlanetWireMode::MountainMask ? "Shaded+MountainMask" : "Shaded+BakedLOD";
    }
}

std::size_t PlanetRenderer::visibleOceanPatchCount() const
{
    return visibleOceanPatches_.size();
}

const PlanetRenderer::CullingStats& PlanetRenderer::cullingStats() const
{
    return lastCullingStats_;
}

const PlanetRenderer::PerformanceStats& PlanetRenderer::performanceStats() const
{
    return lastPerformanceStats_;
}

void PlanetRenderer::updateOceanTessellationBudget(std::size_t oceanPatchCount)
{
    const float pressure = oceanPatchCount > 512
        ? static_cast<float>(oceanPatchCount) / 512.0f
        : 1.0f;
    if (pressure > 1.12f) {
        lodSplitPixelScale_ *= glm::mix(1.0f, 1.08f, glm::clamp((pressure - 1.0f) * 0.75f, 0.0f, 1.0f));
    } else if (pressure < 0.72f) {
        lodSplitPixelScale_ *= 0.96f;
    } else {
        lodSplitPixelScale_ = glm::mix(lodSplitPixelScale_, 1.0f, 0.025f);
    }
    lodSplitPixelScale_ = glm::clamp(lodSplitPixelScale_, 0.85f, 2.35f);
}
std::size_t PlanetRenderer::estimatePatchTriangles(std::size_t patchCount, float tessLevel) const
{
    const float tess = glm::max(tessLevel, 1.0f);
    const float baseQuads = static_cast<float>(kNodeGridResolution * kNodeGridResolution + kNodeGridResolution * 4);
    const float trianglesPerPatch = baseQuads * 2.0f * tess * tess;
    return static_cast<std::size_t>(trianglesPerPatch * static_cast<float>(patchCount));
}

float PlanetRenderer::seaLevelRadius() const
{
    return settings_.planetRadius + settings_.seaLevelOffset * settings_.terrainHeightScale;
}

void PlanetRenderer::TerrainMesh::buildGrid(int patchResolution)
{
    PROFILE_SCOPE("Build Terrain Mesh");
    std::vector<float> vertices;
    std::vector<unsigned> indices;

    const float uvStep = 1.0f / static_cast<float>(patchResolution);

    for (int row = 0; row <= patchResolution; ++row) {
        for (int column = 0; column <= patchResolution; ++column) {
            vertices.push_back(column * uvStep);
            vertices.push_back(row * uvStep);
            vertices.push_back(0.0f);
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

    const auto addVertex = [&](float u, float v, float skirt) {
        const unsigned index = static_cast<unsigned>(vertices.size() / 3);
        vertices.push_back(u);
        vertices.push_back(v);
        vertices.push_back(skirt);
        return index;
    };
    const auto addQuad = [&](unsigned bottomLeft, unsigned bottomRight, unsigned topRight, unsigned topLeft) {
        indices.push_back(bottomLeft);
        indices.push_back(bottomRight);
        indices.push_back(topRight);
        indices.push_back(topLeft);
    };

    for (int column = 0; column < patchResolution; ++column) {
        const float u0 = static_cast<float>(column) * uvStep;
        const float u1 = static_cast<float>(column + 1) * uvStep;
        addQuad(
            addVertex(u0, 0.0f, 1.0f),
            addVertex(u1, 0.0f, 1.0f),
            addVertex(u1, 0.0f, 0.0f),
            addVertex(u0, 0.0f, 0.0f)
        );
        addQuad(
            addVertex(u0, 1.0f, 0.0f),
            addVertex(u1, 1.0f, 0.0f),
            addVertex(u1, 1.0f, 1.0f),
            addVertex(u0, 1.0f, 1.0f)
        );
    }

    for (int row = 0; row < patchResolution; ++row) {
        const float v0 = static_cast<float>(row) * uvStep;
        const float v1 = static_cast<float>(row + 1) * uvStep;
        addQuad(
            addVertex(0.0f, v0, 0.0f),
            addVertex(0.0f, v1, 0.0f),
            addVertex(0.0f, v1, 1.0f),
            addVertex(0.0f, v0, 1.0f)
        );
        addQuad(
            addVertex(1.0f, v0, 1.0f),
            addVertex(1.0f, v1, 1.0f),
            addVertex(1.0f, v1, 0.0f),
            addVertex(1.0f, v0, 0.0f)
        );
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

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void PlanetRenderer::TerrainMesh::draw() const
{
    glBindVertexArray(vertexArrayObject);
    glDrawElements(GL_PATCHES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void PlanetRenderer::BakedTerrainMesh::release()
{
    if (indexBufferObject != 0) {
        glDeleteBuffers(1, &indexBufferObject);
        indexBufferObject = 0;
    }
    if (vertexBufferObject != 0) {
        glDeleteBuffers(1, &vertexBufferObject);
        vertexBufferObject = 0;
    }
    if (vertexArrayObject != 0) {
        glDeleteVertexArrays(1, &vertexArrayObject);
        vertexArrayObject = 0;
    }
    if (lineVertexBufferObject != 0) {
        glDeleteBuffers(1, &lineVertexBufferObject);
        lineVertexBufferObject = 0;
    }
    if (lineVertexArrayObject != 0) {
        glDeleteVertexArrays(1, &lineVertexArrayObject);
        lineVertexArrayObject = 0;
    }
    indexCount = 0;
    maxTerrainRadius = 0.0f;
    chunks.clear();
}

void PlanetRenderer::BakedTerrainMesh::upload(const PlanetProceduralData& proceduralData,
                                              float planetRadius,
                                              float heightScale)
{
    PROFILE_SCOPE("Upload Baked Terrain Mesh");
    release();

    struct GpuVertex {
        glm::vec3 localPos;
        glm::vec3 normal;
        glm::vec3 sphereDir;
        float height;
    };

    std::vector<GpuVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<glm::vec3> lineVertices;
    chunks.reserve(proceduralData.terrainChunks().size());
    maxTerrainRadius = planetRadius;
    std::size_t vertexCount = 0;
    std::size_t indexTotal = 0;
    for (const PlanetProceduralData::TerrainChunk& chunk : proceduralData.terrainChunks()) {
        vertexCount += chunk.vertices.size();
        indexTotal += chunk.indices.size();
    }
    vertices.reserve(vertexCount);
    indices.reserve(indexTotal);

    for (const PlanetProceduralData::TerrainChunk& chunk : proceduralData.terrainChunks()) {
        const std::uint32_t baseVertex = static_cast<std::uint32_t>(vertices.size());
        glm::vec3 localCenter(0.0f);
        float minRadius = planetRadius + chunk.minHeight * heightScale;
        float maxRadius = planetRadius + chunk.maxHeight * heightScale;
        maxTerrainRadius = glm::max(maxTerrainRadius, maxRadius);
        for (const PlanetProceduralData::TerrainChunkVertex& source : chunk.vertices) {
            GpuVertex vertex;
            vertex.localPos = source.sphereDir * (planetRadius + source.height * heightScale);
            vertex.normal = source.normal;
            vertex.sphereDir = source.sphereDir;
            vertex.height = source.height;
            localCenter += vertex.localPos;
            vertices.push_back(vertex);
        }
        if (!chunk.vertices.empty()) {
            localCenter /= static_cast<float>(chunk.vertices.size());
        }

        float radius = glm::abs(maxRadius - minRadius) * 0.5f;
        for (std::size_t i = baseVertex; i < vertices.size(); ++i) {
            radius = glm::max(radius, glm::length(vertices[i].localPos - localCenter));
        }

        ChunkDrawRange drawRange;
        drawRange.localCenter = localCenter;
        drawRange.radius = glm::max(radius + heightScale * 0.05f, 0.001f);

        const auto appendRange = [&](int step, int lodIndex) {
            BakedTerrainMesh::IndexRange range;
            range.firstIndex = indices.size();
            for (std::uint32_t index : chunk.indices) {
                indices.push_back(baseVertex + index);
            }
            range.indexCount = static_cast<GLsizei>(indices.size() - range.firstIndex);
            range.triangleCount = range.indexCount / 3;
            drawRange.lods[static_cast<std::size_t>(lodIndex)] = range;
        };
        appendRange(1, 0);
        appendRange(2, 1);
        appendRange(4, 2);
        chunks.push_back(drawRange);

        if (!chunk.vertices.empty()) {
            const auto localPosAtUvCorner = [&](const glm::vec2& cornerUv) {
                const PlanetProceduralData::TerrainChunkVertex* bestVertex = &chunk.vertices.front();
                float bestDistance = glm::dot(bestVertex->uv - cornerUv, bestVertex->uv - cornerUv);
                for (const PlanetProceduralData::TerrainChunkVertex& vertex : chunk.vertices) {
                    const float distance = glm::dot(vertex.uv - cornerUv, vertex.uv - cornerUv);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        bestVertex = &vertex;
                    }
                }
                return bestVertex->sphereDir * (planetRadius + bestVertex->height * heightScale);
            };
            const glm::vec3 corners[4] = {
                localPosAtUvCorner(chunk.uvMin),
                localPosAtUvCorner(chunk.uvMin + glm::vec2(chunk.uvSize.x, 0.0f)),
                localPosAtUvCorner(chunk.uvMin + chunk.uvSize),
                localPosAtUvCorner(chunk.uvMin + glm::vec2(0.0f, chunk.uvSize.y))
            };
            lineVertices.push_back(corners[0]);
            lineVertices.push_back(corners[1]);
            lineVertices.push_back(corners[1]);
            lineVertices.push_back(corners[2]);
            lineVertices.push_back(corners[2]);
            lineVertices.push_back(corners[3]);
            lineVertices.push_back(corners[3]);
            lineVertices.push_back(corners[0]);
        } else {
            for (int i = 0; i < 8; ++i) {
                lineVertices.push_back(localCenter);
            }
        }
    }

    if (vertices.empty() || indices.empty()) {
        return;
    }

    indexCount = static_cast<GLsizei>(indices.size());
    glGenVertexArrays(1, &vertexArrayObject);
    glGenBuffers(1, &vertexBufferObject);
    glGenBuffers(1, &indexBufferObject);

    glBindVertexArray(vertexArrayObject);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GpuVertex), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, localPos)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, normal)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, sphereDir)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GpuVertex), reinterpret_cast<void*>(offsetof(GpuVertex, height)));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    glGenVertexArrays(1, &lineVertexArrayObject);
    glGenBuffers(1, &lineVertexBufferObject);
    glBindVertexArray(lineVertexArrayObject);
    glBindBuffer(GL_ARRAY_BUFFER, lineVertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(glm::vec3), lineVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void PlanetRenderer::BakedTerrainMesh::draw(const std::vector<VisibleBakedChunk>& visibleChunks) const
{
    if (indexCount <= 0 || visibleChunks.empty()) {
        return;
    }
    glBindVertexArray(vertexArrayObject);
    for (const VisibleBakedChunk& visibleChunk : visibleChunks) {
        if (visibleChunk.chunkIndex >= chunks.size()) {
            continue;
        }
        const ChunkDrawRange& chunk = chunks[visibleChunk.chunkIndex];
        const IndexRange& range = chunk.lods[glm::clamp(static_cast<int>(visibleChunk.lod), 0, 2)];
        glDrawElements(GL_TRIANGLES,
                       range.indexCount,
                       GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(range.firstIndex * sizeof(std::uint32_t)));
    }
    glBindVertexArray(0);
}

void PlanetRenderer::BakedTerrainMesh::drawWire(const std::vector<VisibleBakedChunk>& visibleChunks) const
{
    if (indexCount <= 0 || visibleChunks.empty()) {
        return;
    }
    glBindVertexArray(vertexArrayObject);
    for (const VisibleBakedChunk& visibleChunk : visibleChunks) {
        if (visibleChunk.chunkIndex >= chunks.size()) {
            continue;
        }
        const ChunkDrawRange& chunk = chunks[visibleChunk.chunkIndex];
        const IndexRange& range = chunk.lods[glm::clamp(static_cast<int>(visibleChunk.lod), 0, 2)];
        glDrawElements(GL_TRIANGLES,
                       range.indexCount,
                       GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(range.firstIndex * sizeof(std::uint32_t)));
    }
    glBindVertexArray(0);
}

void PlanetRenderer::BakedTerrainMesh::drawChunkBounds(const std::vector<VisibleBakedChunk>& visibleChunks) const
{
    if (lineVertexArrayObject == 0 || visibleChunks.empty()) {
        return;
    }
    glBindVertexArray(lineVertexArrayObject);
    for (const VisibleBakedChunk& visibleChunk : visibleChunks) {
        if (visibleChunk.chunkIndex >= chunks.size()) {
            continue;
        }
        glDrawArrays(GL_LINES, static_cast<GLint>(visibleChunk.chunkIndex * 8), 8);
    }
    glBindVertexArray(0);
}

void PlanetRenderer::FeatureSegmentMesh::release()
{
    if (vertexBufferObject != 0) {
        glDeleteBuffers(1, &vertexBufferObject);
        vertexBufferObject = 0;
    }
    if (vertexArrayObject != 0) {
        glDeleteVertexArrays(1, &vertexArrayObject);
        vertexArrayObject = 0;
    }
    vertexCount = 0;
    ranges = {};
}

void PlanetRenderer::FeatureSegmentMesh::upload(const PlanetProceduralData& proceduralData,
                                                float,
                                                float)
{
    PROFILE_SCOPE("Upload Feature Segment Mesh");
    release();

    struct FeatureVertex {
        glm::vec3 sphereDir;
        float strength;
    };

    std::vector<FeatureVertex> vertices;
    vertices.reserve(proceduralData.terrainFeatureSegments().size() * 2);

    const auto appendType = [&](PlanetProceduralData::TerrainFeatureType type) {
        const int typeIndex = static_cast<int>(type);
        TypeRange& range = ranges[static_cast<std::size_t>(typeIndex)];
        range.firstVertex = vertices.size();

        for (const PlanetProceduralData::TerrainFeatureSegment& segment : proceduralData.terrainFeatureSegments()) {
            if (segment.type != type) {
                continue;
            }

            const float strength = glm::clamp(segment.strength, 0.0f, 1.0f);
            vertices.push_back({glm::normalize(segment.sphereDirA), strength});
            vertices.push_back({glm::normalize(segment.sphereDirB), strength});
        }

        range.vertexCount = static_cast<GLsizei>(vertices.size() - range.firstVertex);
    };

    appendType(PlanetProceduralData::TerrainFeatureType::River);
    appendType(PlanetProceduralData::TerrainFeatureType::Coast);
    appendType(PlanetProceduralData::TerrainFeatureType::Ridge);
    appendType(PlanetProceduralData::TerrainFeatureType::ErosionEdge);

    if (vertices.empty()) {
        return;
    }

    vertexCount = static_cast<GLsizei>(vertices.size());
    glGenVertexArrays(1, &vertexArrayObject);
    glGenBuffers(1, &vertexBufferObject);

    glBindVertexArray(vertexArrayObject);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(FeatureVertex), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(FeatureVertex), reinterpret_cast<void*>(offsetof(FeatureVertex, sphereDir)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(FeatureVertex), reinterpret_cast<void*>(offsetof(FeatureVertex, strength)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void PlanetRenderer::FeatureSegmentMesh::draw(TerrainFeatureOverlayMode mode) const
{
    if (vertexArrayObject == 0 || vertexCount <= 0 || mode == TerrainFeatureOverlayMode::None) {
        return;
    }

    const auto drawType = [&](PlanetProceduralData::TerrainFeatureType type) {
        const TypeRange& range = ranges[static_cast<std::size_t>(type)];
        if (range.vertexCount <= 0) {
            return;
        }
        glDrawArrays(GL_LINES, static_cast<GLint>(range.firstVertex), range.vertexCount);
    };

    glBindVertexArray(vertexArrayObject);
    switch (mode) {
    case TerrainFeatureOverlayMode::All:
        drawType(PlanetProceduralData::TerrainFeatureType::River);
        drawType(PlanetProceduralData::TerrainFeatureType::Coast);
        drawType(PlanetProceduralData::TerrainFeatureType::Ridge);
        drawType(PlanetProceduralData::TerrainFeatureType::ErosionEdge);
        break;
    case TerrainFeatureOverlayMode::Rivers:
        drawType(PlanetProceduralData::TerrainFeatureType::River);
        break;
    case TerrainFeatureOverlayMode::Coast:
        drawType(PlanetProceduralData::TerrainFeatureType::Coast);
        break;
    case TerrainFeatureOverlayMode::Ridges:
        drawType(PlanetProceduralData::TerrainFeatureType::Ridge);
        break;
    case TerrainFeatureOverlayMode::Erosion:
        drawType(PlanetProceduralData::TerrainFeatureType::ErosionEdge);
        break;
    case TerrainFeatureOverlayMode::None:
    default:
        break;
    }
    glBindVertexArray(0);
}

void PlanetRenderer::SphereMesh::buildSphere(int longitudeSegments, int latitudeSegments)
{
    PROFILE_SCOPE("Build Atmosphere Mesh");
    longitudeSegments = glm::max(longitudeSegments, 8);
    latitudeSegments = glm::max(latitudeSegments, 4);

    std::vector<float> vertices;
    std::vector<unsigned> indices;
    vertices.reserve(static_cast<std::size_t>((longitudeSegments + 1) * (latitudeSegments + 1) * 3));

    constexpr float pi = 3.14159265358979323846f;
    for (int lat = 0; lat <= latitudeSegments; ++lat) {
        const float v = static_cast<float>(lat) / static_cast<float>(latitudeSegments);
        const float theta = v * pi;
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);

        for (int lon = 0; lon <= longitudeSegments; ++lon) {
            const float u = static_cast<float>(lon) / static_cast<float>(longitudeSegments);
            const float phi = u * pi * 2.0f;
            vertices.push_back(sinTheta * std::cos(phi));
            vertices.push_back(cosTheta);
            vertices.push_back(sinTheta * std::sin(phi));
        }
    }

    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            const unsigned current = static_cast<unsigned>(lat * (longitudeSegments + 1) + lon);
            const unsigned next = static_cast<unsigned>((lat + 1) * (longitudeSegments + 1) + lon);

            indices.push_back(current);
            indices.push_back(current + 1);
            indices.push_back(next);

            indices.push_back(current + 1);
            indices.push_back(next + 1);
            indices.push_back(next);
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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void PlanetRenderer::SphereMesh::draw() const
{
    glBindVertexArray(vertexArrayObject);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

glm::vec3 PlanetRenderer::cubeSphereDirection(const FaceBasis& face, const glm::vec2& uv)
{
    const glm::vec2 faceUv = uv * 2.0f - 1.0f;
    return glm::normalize(face.normal + faceUv.x * face.axisU + faceUv.y * face.axisV);
}

int PlanetRenderer::faceIndexFromDirection(const glm::vec3& direction)
{
    const glm::vec3 d = glm::normalize(direction);
    const glm::vec3 a = glm::abs(d);
    if (a.x >= a.y && a.x >= a.z) {
        return d.x >= 0.0f ? 0 : 1;
    }
    if (a.y >= a.x && a.y >= a.z) {
        return d.y >= 0.0f ? 2 : 3;
    }
    return d.z >= 0.0f ? 4 : 5;
}

glm::vec2 PlanetRenderer::faceUvFromDirection(int faceIndex, const glm::vec3& direction)
{
    const glm::vec3 d = glm::normalize(direction);
    const FaceBasis& basis = kPlanetFaces[static_cast<std::size_t>(faceIndex)];
    const float projection = std::max(std::abs(glm::dot(d, basis.normal)), 0.000001f);
    const glm::vec3 cubePoint = d / projection;
    const glm::vec2 faceUv(
        glm::dot(cubePoint - basis.normal, basis.axisU),
        glm::dot(cubePoint - basis.normal, basis.axisV)
    );
    return glm::clamp(faceUv * 0.5f + 0.5f, glm::vec2(0.0f), glm::vec2(0.999999f));
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

PlanetRenderer::NodeBounds PlanetRenderer::computeNodeBounds(const FaceBasis& face, const QuadtreeNode& node) const
{
    const float sampleRadius = settings_.planetRadius + settings_.terrainHeightScale * 2.0f;
    const glm::vec3 centerDirection = nodeCenterDirection(face, node);
    const glm::vec3 center = centerDirection * sampleRadius;

    float patchRadius = 0.0f;
    float lodScale = 1.0f;
    for (int y = 0; y <= 2; ++y) {
        for (int x = 0; x <= 2; ++x) {
            const glm::vec2 uv = node.uvMin + glm::vec2(
                node.uvSize * static_cast<float>(x) * 0.5f,
                node.uvSize * static_cast<float>(y) * 0.5f
            );
            const glm::vec3 dir = cubeSphereDirection(face, uv);
            const glm::vec3 absDir = glm::abs(dir);
            const float maxAxis = glm::max(glm::max(absDir.x, absDir.y), absDir.z);
            const float cubeSphereScale = 1.0f / glm::max(maxAxis, 0.0001f);
            patchRadius = glm::max(patchRadius, glm::length(dir * sampleRadius - center));
            lodScale = glm::max(lodScale, cubeSphereScale);
        }
    }

    NodeBounds bounds;
    bounds.worldDirection = worldDirection(centerDirection);
    bounds.radius = glm::max(patchRadius, 0.001f);
    bounds.lodScale = glm::clamp(lodScale, 1.0f, 1.75f);
    return bounds;
}

bool PlanetRenderer::isNodeOutsideFrustum(const Frustum& frustum,
                                          const NodeBounds& bounds) const
{
    const float boundsRadius = settings_.planetRadius
                             + settings_.terrainHeightScale * 2.0f
                             + glm::abs(settings_.seaLevelOffset * settings_.terrainHeightScale);
    const glm::vec3 center = bounds.worldDirection * boundsRadius;
    const float radius = bounds.radius + settings_.terrainHeightScale * 2.0f;

    for (const glm::vec4& plane : frustum.planes) {
        const float signedDistance = glm::dot(glm::vec3(plane), center) + plane.w;
        if (signedDistance < -radius) {
            return true;
        }
    }

    return false;
}

bool PlanetRenderer::isNodeHiddenByHorizon(const FlyCamera& camera,
                                           const NodeBounds& bounds) const
{
    const float cameraDistanceFromOrigin = glm::length(camera.position);
    if (cameraDistanceFromOrigin <= settings_.planetRadius + settings_.terrainHeightScale * 2.0f) {
        return false;
    }

    const glm::vec3 cameraDirection = glm::normalize(camera.position);
    const float horizonDot = settings_.planetRadius / cameraDistanceFromOrigin;
    const float safetyMargin = bounds.radius / settings_.planetRadius;
    return glm::dot(cameraDirection, bounds.worldDirection) < horizonDot - safetyMargin;
}

bool PlanetRenderer::isSphereOutsideFrustum(const Frustum& frustum, const glm::vec3& worldCenter, float radius) const
{
    for (const glm::vec4& plane : frustum.planes) {
        const float signedDistance = glm::dot(glm::vec3(plane), worldCenter) + plane.w;
        if (signedDistance < -radius) {
            return true;
        }
    }
    return false;
}

bool PlanetRenderer::isSphereHiddenByHorizon(const FlyCamera& camera, const glm::vec3& worldCenter, float radius) const
{
    const float cameraDistanceFromOrigin = glm::length(camera.position);
    if (cameraDistanceFromOrigin <= settings_.planetRadius + settings_.terrainHeightScale * 2.0f) {
        return false;
    }
    const float centerDistance = glm::length(worldCenter);
    if (centerDistance <= 0.0001f) {
        return false;
    }

    const glm::vec3 cameraDirection = glm::normalize(camera.position);
    const glm::vec3 centerDirection = worldCenter / centerDistance;
    const float horizonDot = settings_.planetRadius / cameraDistanceFromOrigin;
    const float safetyMargin = radius / glm::max(settings_.planetRadius, 0.0001f);
    return glm::dot(cameraDirection, centerDirection) < horizonDot - safetyMargin;
}

PlanetRenderer::PatchWaterCoverage PlanetRenderer::analyzePatchWaterCoverage(int faceIndex,
                                                                              const glm::vec2& patchUvMin,
                                                                              const glm::vec2& patchUvSize) const
{
    PatchWaterCoverage coverage;
    if (!hasProceduralOceanData_
        || proceduralDataResolution_ <= 0
        || proceduralWaterCoveragePrefixCpu_.empty()
        || proceduralShoreCoverageLoosePrefixCpu_.empty()
        || proceduralShoreCoverageStrictPrefixCpu_.empty()
        || faceIndex < 0
        || faceIndex >= 6) {
        return coverage;
    }

    const glm::vec2 uvMin = glm::clamp(patchUvMin, glm::vec2(0.0f), glm::vec2(1.0f));
    const glm::vec2 uvMax = glm::clamp(patchUvMin + patchUvSize, glm::vec2(0.0f), glm::vec2(1.0f));
    const glm::vec2 uvExtent = uvMax - uvMin;
    if (uvExtent.x <= 0.0f || uvExtent.y <= 0.0f) {
        return coverage;
    }

    const int resolution = proceduralDataResolution_;
    const int prefixStride = resolution + 1;
    const std::size_t prefixLayerSize = static_cast<std::size_t>(prefixStride * prefixStride);
    const std::size_t prefixOffset = static_cast<std::size_t>(faceIndex) * prefixLayerSize;
    const auto queryPrefix = [&](const std::vector<std::uint32_t>& prefix, int x0, int y0, int x1, int y1) {
        const std::size_t topLeft = prefixOffset + static_cast<std::size_t>(y0 * prefixStride + x0);
        const std::size_t topRight = prefixOffset + static_cast<std::size_t>(y0 * prefixStride + x1);
        const std::size_t bottomLeft = prefixOffset + static_cast<std::size_t>(y1 * prefixStride + x0);
        const std::size_t bottomRight = prefixOffset + static_cast<std::size_t>(y1 * prefixStride + x1);
        return prefix[bottomRight] - prefix[bottomLeft] - prefix[topRight] + prefix[topLeft];
    };

    const int x0 = glm::clamp(static_cast<int>(std::floor(uvMin.x * static_cast<float>(resolution))) - 1, 0, resolution);
    const int y0 = glm::clamp(static_cast<int>(std::floor(uvMin.y * static_cast<float>(resolution))) - 1, 0, resolution);
    const int x1 = glm::clamp(static_cast<int>(std::ceil(uvMax.x * static_cast<float>(resolution))) + 1, 0, resolution);
    const int y1 = glm::clamp(static_cast<int>(std::ceil(uvMax.y * static_cast<float>(resolution))) + 1, 0, resolution);
    if (x0 >= x1 || y0 >= y1) {
        return coverage;
    }

    const std::uint32_t coveredTexels = static_cast<std::uint32_t>((x1 - x0) * (y1 - y0));
    const std::uint32_t waterTexels = queryPrefix(proceduralWaterCoveragePrefixCpu_, x0, y0, x1, y1);
    const std::uint32_t looseShoreTexels = queryPrefix(proceduralShoreCoverageLoosePrefixCpu_, x0, y0, x1, y1);
    const std::uint32_t strictShoreTexels = queryPrefix(proceduralShoreCoverageStrictPrefixCpu_, x0, y0, x1, y1);

    coverage.hasData = true;
    coverage.hasWater = waterTexels > 0;
    coverage.hasLand = waterTexels < coveredTexels;
    coverage.maxShoreMask = strictShoreTexels > 0 ? 0.02f : (looseShoreTexels > 0 ? 0.002f : 0.0f);

    return coverage;
}

bool PlanetRenderer::shouldSplitNode(const FlyCamera& camera,
                                     const NodeBounds& bounds,
                                     int nodeDepth,
                                     int framebufferHeight) const
{
    if (nodeDepth < kMinimumLodDepth) return true;
    if (nodeDepth >= kMaximumLodDepth) return false;

    const glm::vec3 patchCenter = bounds.worldDirection * settings_.planetRadius;
    const float centerDistanceToCamera = glm::length(camera.position - patchCenter);
    const float distanceToCamera = glm::max(centerDistanceToCamera - bounds.radius, 0.001f);
    const float projectionScale = (0.5f * static_cast<float>(framebufferHeight))
                                / glm::tan(glm::radians(camera.fieldOfView) * 0.5f);
    const float projectedRadius = bounds.radius * bounds.lodScale * projectionScale / distanceToCamera;
    return projectedRadius > kLodSplitPixels * lodSplitPixelScale_;
}

void PlanetRenderer::collectVisibleOceanPatches(const FlyCamera& camera,
                                                const Frustum& frustum,
                                                int faceIndex,
                                                const QuadtreeNode& node,
                                                int framebufferHeight,
                                                CullingStats& stats,
                                                std::vector<OceanPatch>& outPatches) const
{
    const FaceBasis& face = kPlanetFaces[faceIndex];
    const NodeBounds bounds = computeNodeBounds(face, node);
    ++stats.visitedNodes;

    if (isNodeOutsideFrustum(frustum, bounds)) {
        ++stats.frustumCulledNodes;
        return;
    }

    if (isNodeHiddenByHorizon(camera, bounds)) {
        ++stats.horizonCulledNodes;
        return;
    }

    PatchWaterCoverage nodeCoverage;
    bool hasNodeCoverage = false;
    bool forceShoreLod = false;
    if (node.depth < kShoreMinimumLodDepth) {
        nodeCoverage = analyzePatchWaterCoverage(faceIndex, node.uvMin, glm::vec2(node.uvSize));
        hasNodeCoverage = nodeCoverage.hasData;
        forceShoreLod = nodeCoverage.maxShoreMask > 0.015f || (nodeCoverage.hasWater && nodeCoverage.hasLand);
    }
    if (forceShoreLod || shouldSplitNode(camera, bounds, node.depth, framebufferHeight)) {
        ++stats.splitNodes;
        const float childSize = node.uvSize * 0.5f;

        for (int childY = 0; childY < 2; ++childY) {
            for (int childX = 0; childX < 2; ++childX) {
                QuadtreeNode childNode;
                childNode.uvMin = node.uvMin + glm::vec2(childX * childSize, childY * childSize);
                childNode.uvSize = childSize;
                childNode.depth = node.depth + 1;
                collectVisibleOceanPatches(camera, frustum, faceIndex, childNode, framebufferHeight, stats, outPatches);
            }
        }
        return;
    }

    OceanPatch patch;
    patch.faceIndex = faceIndex;
    patch.uvMin = node.uvMin;
    patch.uvSize = glm::vec2(node.uvSize);
    patch.depth = node.depth;
    patch.waterCoverage = hasNodeCoverage
        ? nodeCoverage
        : analyzePatchWaterCoverage(faceIndex, patch.uvMin, patch.uvSize);
    ++stats.emittedPatches;
    outPatches.push_back(patch);
}

std::vector<PlanetRenderer::OceanPatch> PlanetRenderer::buildVisibleOceanPatches(const FlyCamera& camera,
                                                                                 const Frustum& frustum,
                                                                                 int framebufferHeight,
                                                                                 CullingStats& stats) const
{
    PROFILE_SCOPE("Build Visible Terrain Patches");
    std::vector<OceanPatch> patches;
    patches.reserve(256);

    const QuadtreeNode rootNode;
    for (int faceIndex = 0; faceIndex < static_cast<int>(kPlanetFaces.size()); ++faceIndex) {
        collectVisibleOceanPatches(camera, frustum, faceIndex, rootNode, framebufferHeight, stats, patches);
    }

    std::vector<OceanPatch> oceanPatches;
    oceanPatches.reserve(patches.size());
    for (const OceanPatch& patch : patches) {
        if (patchHasOceanCoverage(patch)) {
            oceanPatches.push_back(patch);
        }
    }
    stats.emittedPatches = oceanPatches.size();
    return oceanPatches;
}

std::vector<PlanetRenderer::VisibleBakedChunk> PlanetRenderer::buildVisibleBakedChunks(const FlyCamera& camera,
                                                                                       const Frustum& frustum,
                                                                                       int framebufferHeight,
                                                                                       CullingStats& stats) const
{
    PROFILE_SCOPE("Build Visible Baked Chunks");
    std::vector<VisibleBakedChunk> visibleChunks;
    visibleChunks.reserve(bakedTerrainMesh_.chunks.size());

    const glm::mat3 modelRotation(modelMatrix_);
    const float projectionScale = (0.5f * static_cast<float>(glm::max(framebufferHeight, 1)))
                                / glm::tan(glm::radians(camera.fieldOfView) * 0.5f);
    for (std::size_t chunkIndex = 0; chunkIndex < bakedTerrainMesh_.chunks.size(); ++chunkIndex) {
        const BakedTerrainMesh::ChunkDrawRange& chunk = bakedTerrainMesh_.chunks[chunkIndex];
        ++stats.visitedNodes;

        const glm::vec3 worldCenter = modelRotation * chunk.localCenter;
        if (isSphereOutsideFrustum(frustum, worldCenter, chunk.radius)) {
            ++stats.frustumCulledNodes;
            continue;
        }
        if (isSphereHiddenByHorizon(camera, worldCenter, chunk.radius)) {
            ++stats.horizonCulledNodes;
            continue;
        }

        const float distanceToCamera = glm::max(glm::length(camera.position - worldCenter) - chunk.radius, 0.001f);
        const float projectedRadius = chunk.radius * projectionScale / distanceToCamera;
        const std::uint8_t lod = projectedRadius > 90.0f
            ? 0
            : (projectedRadius > 35.0f ? 1 : 2);

        visibleChunks.push_back(VisibleBakedChunk{
            static_cast<std::uint32_t>(chunkIndex),
            lod
        });
    }

    stats.emittedPatches = visibleChunks.size();
    return visibleChunks;
}


bool PlanetRenderer::patchHasOceanCoverage(const OceanPatch& patch) const
{
    const PatchWaterCoverage& coverage = patch.waterCoverage;
    if (!coverage.hasData) {
        return true;
    }

    return coverage.hasWater || coverage.maxShoreMask > 0.001f;
}

void PlanetRenderer::applyCommonUniforms(const ShaderProgram& program,
                                         const FlyCamera& camera,
                                         const glm::mat4& viewMatrix,
                                         const glm::mat4& projectionMatrix,
                                         const OceanPatch& patch) const
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
    program.setFloat("tessMin", settings_.oceanTessellationMin);
    program.setFloat("tessMax", effectiveOceanTessellationMax_);
    program.setFloat("tessMinDist", settings_.oceanTessellationNearDistance);
    program.setFloat("tessMaxDist", settings_.oceanTessellationFarDistance);
    program.setFloat("planetRadius", settings_.planetRadius);
    program.setFloat("seaLevelRadius", seaLevelRadius());
    program.setFloat("heightScale", settings_.terrainHeightScale);
    program.setFloat("seaLevelOffset", settings_.seaLevelOffset);
    program.setFloat("runtimeMountainScale", settings_.runtimeMountainScale);
    program.setVec3("terrainLowlandColor", settings_.terrainLowlandColor);
    program.setVec3("terrainForestColor", settings_.terrainForestColor);
    program.setVec3("terrainDesertColor", settings_.terrainDesertColor);
    program.setVec3("terrainRockColor", settings_.terrainRockColor);
    program.setVec3("terrainBeachColor", settings_.terrainBeachColor);
    program.setVec3("terrainSnowColor", settings_.terrainSnowColor);
    program.setVec3("terrainPaletteLowGrass", settings_.terrainPaletteLowGrass);
    program.setVec3("terrainPaletteMeadow", settings_.terrainPaletteMeadow);
    program.setVec3("terrainPaletteForestDark", settings_.terrainPaletteForestDark);
    program.setVec3("terrainPaletteForestWarm", settings_.terrainPaletteForestWarm);
    program.setVec3("terrainPaletteSavanna", settings_.terrainPaletteSavanna);
    program.setVec3("terrainPaletteDrySoil", settings_.terrainPaletteDrySoil);
    program.setVec3("terrainPaletteOchre", settings_.terrainPaletteOchre);
    program.setVec3("terrainPaletteWetGreen", settings_.terrainPaletteWetGreen);
    program.setVec3("terrainPaletteTundra", settings_.terrainPaletteTundra);
    program.setVec3("terrainPaletteBrownSlope", settings_.terrainPaletteBrownSlope);
    program.setVec3("terrainPaletteRedSoil", settings_.terrainPaletteRedSoil);
    program.setVec3("terrainPaletteRockWarm", settings_.terrainPaletteRockWarm);
    program.setVec3("terrainPaletteRockCool", settings_.terrainPaletteRockCool);
    program.setVec3("terrainPaletteRockDark", settings_.terrainPaletteRockDark);
    program.setVec3("terrainPalettePaleStone", settings_.terrainPalettePaleStone);
    program.setVec3("terrainPaletteSnow", settings_.terrainPaletteSnow);
    program.setVec3("terrainPaletteSnowShadow", settings_.terrainPaletteSnowShadow);
    program.setVec3("terrainPaletteBeach", settings_.terrainPaletteBeach);
    program.setVec3("terrainPaletteRiverBed", settings_.terrainPaletteRiverBed);
    program.setVec3("terrainPaletteShallowSeabed", settings_.terrainPaletteShallowSeabed);
    program.setVec3("terrainPaletteDeepSeabed", settings_.terrainPaletteDeepSeabed);
    program.setFloat("terrainBeachWidth", settings_.terrainBeachWidth);
    program.setFloat("terrainRockSlopeStart", settings_.terrainRockSlopeStart);
    program.setFloat("terrainRockSlopeEnd", settings_.terrainRockSlopeEnd);
    program.setFloat("terrainSnowStart", settings_.terrainSnowStart);
    program.setFloat("terrainSnowEnd", settings_.terrainSnowEnd);
    program.setFloat("terrainMaterialNoiseScale", settings_.terrainMaterialNoiseScale);
    program.setFloat("terrainMaterialNoiseStrength", settings_.terrainMaterialNoiseStrength);
    program.setInt("renderRivers", settings_.renderRivers ? 1 : 0);
    program.setFloat("riverVisibility", settings_.riverVisibility);
    program.setFloat("riverWidth", settings_.riverWidth);
    program.setFloat("riverShine", settings_.riverShine);
    program.setFloat("riverRefractionStrength", settings_.riverRefractionStrength);
    program.setVec3("riverColor", settings_.riverColor);
    program.setFloat("timeSeconds", currentTimeSeconds_);
    program.setFloat("gridCount", static_cast<float>(kNodeGridResolution));
    program.setFloat("coarseLineWidth", settings_.coarseGridLineWidth);
    program.setVec3("skyColor", settings_.skyColor);
    program.setFloat("fogDensity", settings_.fogDensity);
    const float atmosphereRadius = settings_.planetRadius + glm::max(settings_.atmosphereHeight, 0.0f);
    program.setInt("renderAtmosphere", settings_.renderAtmosphere ? 1 : 0);
    program.setFloat("atmosphereRadius", atmosphereRadius);
    program.setFloat("atmosphereDensity", settings_.atmosphereDensity);
    program.setFloat("atmosphereExposure", settings_.atmosphereExposure);
    program.setFloat("scatteringViewMuSize", static_cast<float>(atmosphereLut_.scatteringViewMuSize));
    program.setFloat("scatteringNuSize", static_cast<float>(atmosphereLut_.scatteringNuSize));
    program.setFloat("scatteringHeight", static_cast<float>(atmosphereLut_.scatteringHeight));
    program.setFloat("scatteringDepth", static_cast<float>(atmosphereLut_.scatteringDepth));
    program.setVec3("mieColor", settings_.atmosphereMieColor);
    program.setFloat("cameraNearPlane", settings_.cameraNearPlane);
    program.setFloat("cameraFarPlane", settings_.cameraFarPlane);
    program.setFloat("oceanAlpha", settings_.oceanAlpha);
    program.setFloat("oceanShallowAlpha", settings_.oceanShallowAlpha);
    program.setFloat("oceanDeepAlpha", settings_.oceanDeepAlpha);
    const bool oceanMaterialEnabled = settings_.renderOceanMaterial;
    const bool oceanWavesEnabled = settings_.renderOceanWaves;
    program.setFloat("oceanFresnelStrength", oceanMaterialEnabled ? settings_.oceanFresnelStrength : 0.0f);
    program.setFloat("oceanDistortionStrength", oceanMaterialEnabled ? settings_.oceanDistortionStrength : 0.0f);
    program.setFloat("oceanDepthRange", oceanMaterialEnabled ? settings_.oceanDepthRange : 40.0f);
    program.setFloat("oceanShallowDepthRange", oceanMaterialEnabled ? settings_.oceanShallowDepthRange : 4.0f);
    program.setFloat("oceanDepthScale", oceanMaterialEnabled ? settings_.oceanDepthScale : 1.0f);
    program.setFloat("oceanTintStrength", oceanMaterialEnabled ? settings_.oceanTintStrength : 0.0f);
    program.setFloat("oceanWaveAmplitude", oceanWavesEnabled ? settings_.oceanWaveAmplitude : 0.0f);
    program.setFloat("oceanChoppiness", oceanWavesEnabled ? settings_.oceanChoppiness : 0.0f);
    program.setFloat("oceanWaveTileScale", settings_.oceanWaveTileScale);
    program.setFloat("oceanHeightTexelSize", fftOcean_.texelSize());
    program.setFloat("oceanWaveNormalStrength", oceanWavesEnabled ? settings_.oceanWaveNormalStrength : 0.0f);
    program.setFloat("oceanDetailNormalStrength", oceanMaterialEnabled ? settings_.oceanDetailNormalStrength : 0.0f);
    program.setFloat("oceanDetailNormalScale", settings_.oceanDetailNormalScale);
    program.setFloat("oceanDetailFadeDistance", settings_.oceanDetailFadeDistance);
    program.setFloat("oceanSpecularStrength", oceanMaterialEnabled ? settings_.oceanSpecularStrength : 0.0f);
    program.setFloat("oceanSpecularSharpness", settings_.oceanSpecularSharpness);
    program.setFloat("oceanRoughness", oceanMaterialEnabled ? settings_.oceanRoughness : 0.35f);
    program.setFloat("oceanSSSStrength", oceanMaterialEnabled ? settings_.oceanSSSStrength : 0.0f);
    program.setFloat("oceanSSSPower", settings_.oceanSSSPower);
    program.setFloat("oceanShoreBlendWidth", settings_.oceanShoreBlendWidth);
    program.setFloat("oceanReflectionWeight", oceanReflectionWeight_);
    program.setFloat("oceanRefractionWeight", oceanRefractionWeight_);
    program.setInt("renderMode", static_cast<int>(settings_.renderMode));
    program.setInt("useProceduralOceanData", hasProceduralOceanData_ ? 1 : 0);
    program.setInt("useProceduralData", hasProceduralOceanData_ ? 1 : 0);
    program.setFloat("proceduralDataTexelSize", proceduralDataResolution_ > 0 ? 1.0f / static_cast<float>(proceduralDataResolution_) : 0.0f);
    program.setVec2("nodeUvMin", patch.uvMin);
    program.setVec2("nodeUvSize", patch.uvSize);
    program.setVec3("oceanShallowColor", settings_.oceanShallowColor);
    program.setVec3("oceanDeepColor", settings_.oceanDeepColor);
    program.setVec3("oceanSSSColor", settings_.oceanSSSColor);
    program.setVec3("faceNormal", face.normal);
    program.setVec3("faceAxisU", face.axisU);
    program.setVec3("faceAxisV", face.axisV);
}

void PlanetRenderer::drawTerrainPass(const FlyCamera& camera,
                                     const glm::mat4& viewMatrix,
                                     const glm::mat4& projectionMatrix)
{
    if (!settings_.renderTerrain || bakedTerrainMesh_.indexCount <= 0) {
        return;
    }
    drawBakedTerrainPass(camera, viewMatrix, projectionMatrix, false, 0.0f, true);
}

void PlanetRenderer::drawTerrainPass(const FlyCamera& camera,
                                     const glm::mat4& viewMatrix,
                                     const glm::mat4& projectionMatrix,
                                     bool useClipPlane,
                                     float clipPlaneY,
                                     bool keepAboveClipPlane)
{
    if (settings_.renderTerrain && bakedTerrainMesh_.indexCount > 0) {
        drawBakedTerrainPass(camera, viewMatrix, projectionMatrix, useClipPlane, clipPlaneY, keepAboveClipPlane);
    }
}

void PlanetRenderer::drawBakedTerrainPass(const FlyCamera& camera,
                                          const glm::mat4& viewMatrix,
                                          const glm::mat4& projectionMatrix,
                                          bool useClipPlane,
                                          float clipPlaneY,
                                          bool keepAboveClipPlane)
{
    PROFILE_SCOPE(useClipPlane ? "Draw Baked Terrain Clip Pass" : "Draw Baked Terrain Pass");

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (useClipPlane) {
        glEnable(GL_CLIP_DISTANCE0);
    } else {
        glDisable(GL_CLIP_DISTANCE0);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralWaterDepthTexture_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralHeightTexture_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralTemperatureTexture_);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralMoistureTexture_);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralErosionMaskTexture_);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralBiomeWeightATexture_);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralBiomeWeightBTexture_);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralDomainWeightTexture_);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, atmosphereLut_.irradianceTexture);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_3D, atmosphereLut_.scatteringTexture);

    OceanPatch dummyPatch;
    dummyPatch.faceIndex = 0;
    dummyPatch.uvMin = glm::vec2(0.0f);
    dummyPatch.uvSize = glm::vec2(1.0f);
    applyCommonUniforms(terrainChunkProgram_, camera, viewMatrix, projectionMatrix, dummyPatch);
    terrainChunkProgram_.setInt("proceduralWaterDepthTexture", 0);
    terrainChunkProgram_.setInt("proceduralHeightTexture", 1);
    terrainChunkProgram_.setInt("proceduralTemperatureTexture", 2);
    terrainChunkProgram_.setInt("proceduralMoistureTexture", 3);
    terrainChunkProgram_.setInt("proceduralErosionMaskTexture", 5);
    terrainChunkProgram_.setInt("proceduralBiomeWeightATexture", 6);
    terrainChunkProgram_.setInt("proceduralBiomeWeightBTexture", 7);
    terrainChunkProgram_.setInt("proceduralDomainWeightTexture", 8);
    terrainChunkProgram_.setInt("atmosphereIrradianceTexture", 11);
    terrainChunkProgram_.setInt("atmosphereScatteringTexture", 12);
    terrainChunkProgram_.setInt("terrainDebugOverlayMode",
                                settings_.wireMode == PlanetWireMode::MountainMask ? 1 : 0);
    terrainChunkProgram_.setVec4("clipPlane", useClipPlane ? glm::vec4(0.0f, keepAboveClipPlane ? 1.0f : -1.0f, 0.0f, -clipPlaneY)
                                                           : glm::vec4(0.0f, 0.0f, 0.0f, -1.0e9f));
    bakedTerrainMesh_.draw(visibleBakedChunks_);
    glDisable(GL_CLIP_DISTANCE0);
}

void PlanetRenderer::drawOceanPass(const FlyCamera& camera,
                                   const glm::mat4& viewMatrix,
                                   const glm::mat4& projectionMatrix)
{
    PROFILE_SCOPE("Draw Ocean Pass");
    if (!settings_.renderOcean) {
        return;
    }
    if (visibleOceanPatches_.empty()) {
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
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.displacementTexture());
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralHeightTexture_);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralWaterDepthTexture_);
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, atmosphereLut_.irradianceTexture);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_3D, atmosphereLut_.scatteringTexture);

    for (const OceanPatch& patch : visibleOceanPatches_) {
        applyCommonUniforms(oceanProgram_, camera, viewMatrix, projectionMatrix, patch);
        oceanProgram_.setFloat("tessMin", settings_.oceanTessellationMin);
        oceanProgram_.setFloat("tessMax", effectiveOceanTessellationMax_);
        oceanProgram_.setFloat("tessMinDist", settings_.oceanTessellationNearDistance);
        oceanProgram_.setFloat("tessMaxDist", settings_.oceanTessellationFarDistance);
        oceanProgram_.setInt("reflectionTexture", 0);
        oceanProgram_.setInt("refractionTexture", 1);
        oceanProgram_.setInt("refractionDepthTexture", 2);
        oceanProgram_.setInt("oceanHeightTexture", 3);
        oceanProgram_.setInt("oceanNormalTexture", 4);
        oceanProgram_.setInt("waterDetailNormalTextureA", 5);
        oceanProgram_.setInt("waterDetailNormalTextureB", 6);
        oceanProgram_.setInt("oceanDisplacementTexture", 8);
        oceanProgram_.setInt("proceduralHeightTexture", 9);
        oceanProgram_.setInt("proceduralWaterDepthTexture", 10);
        oceanProgram_.setInt("atmosphereIrradianceTexture", 11);
        oceanProgram_.setInt("atmosphereScatteringTexture", 12);
        terrainMesh_.draw();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

std::uint64_t PlanetRenderer::computeAtmosphereLutSignature() const
{
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    const auto mixFloat = [&](float value) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        mix(bits);
    };
    const auto mixVec3 = [&](const glm::vec3& value) {
        mixFloat(value.x);
        mixFloat(value.y);
        mixFloat(value.z);
    };

    mixFloat(settings_.atmosphereHeight);
    mixFloat(settings_.atmosphereDensity);
    mixFloat(settings_.atmosphereRayleighStrength);
    mixFloat(settings_.atmosphereMieStrength);
    mixFloat(settings_.atmosphereMieAnisotropy);
    mixFloat(settings_.atmosphereExposure);
    mixVec3(settings_.atmosphereRayleighColor);
    mixVec3(settings_.atmosphereMieColor);
    mix(static_cast<std::uint64_t>(atmosphereLut_.transmittanceWidth));
    mix(static_cast<std::uint64_t>(atmosphereLut_.transmittanceHeight));
    mix(static_cast<std::uint64_t>(atmosphereLut_.irradianceWidth));
    mix(static_cast<std::uint64_t>(atmosphereLut_.irradianceHeight));
    mix(static_cast<std::uint64_t>(atmosphereLut_.scatteringViewMuSize));
    mix(static_cast<std::uint64_t>(atmosphereLut_.scatteringNuSize));
    mix(static_cast<std::uint64_t>(atmosphereLut_.scatteringHeight));
    mix(static_cast<std::uint64_t>(atmosphereLut_.scatteringDepth));
    mix(static_cast<std::uint64_t>(atmosphereLut_.scatteringOrderCount));
    return hash == 0 ? 1 : hash;
}

void PlanetRenderer::precomputeAtmosphereLuts()
{
    PROFILE_SCOPE("Precompute Atmosphere LUTs");
    atmosphereLut_.create();
    if (atmosphereLut_.framebufferObject == 0
        || atmosphereLut_.transmittanceTexture == 0
        || atmosphereLut_.irradianceTexture == 0
        || atmosphereLut_.scatteringTexture == 0
        || atmosphereLut_.deltaScatteringTextureA == 0
        || atmosphereLut_.deltaScatteringTextureB == 0
        || fullscreenVertexArrayObject_ == 0) {
        return;
    }

    const std::uint64_t signature = computeAtmosphereLutSignature();
    if (atmosphereLut_.cachedSignature == signature) {
        return;
    }

    GLint previousFramebuffer = 0;
    GLint previousViewport[4] = {};
    GLint previousActiveTexture = GL_TEXTURE0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, atmosphereLut_.framebufferObject);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(fullscreenVertexArrayObject_);

    atmosphereTransmittanceProgram_.use();
    atmosphereTransmittanceProgram_.setVec3("rayleighColor", settings_.atmosphereRayleighColor);
    atmosphereTransmittanceProgram_.setVec3("mieColor", settings_.atmosphereMieColor);
    atmosphereTransmittanceProgram_.setFloat("atmosphereDensity", settings_.atmosphereDensity);
    atmosphereTransmittanceProgram_.setFloat("rayleighStrength", settings_.atmosphereRayleighStrength);
    atmosphereTransmittanceProgram_.setFloat("mieStrength", settings_.atmosphereMieStrength);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, atmosphereLut_.transmittanceTexture, 0);
    glViewport(0, 0, atmosphereLut_.transmittanceWidth, atmosphereLut_.transmittanceHeight);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    atmosphereIrradianceProgram_.use();
    atmosphereIrradianceProgram_.setVec3("rayleighColor", settings_.atmosphereRayleighColor);
    atmosphereIrradianceProgram_.setVec3("mieColor", settings_.atmosphereMieColor);
    atmosphereIrradianceProgram_.setFloat("atmosphereDensity", settings_.atmosphereDensity);
    atmosphereIrradianceProgram_.setFloat("rayleighStrength", settings_.atmosphereRayleighStrength);
    atmosphereIrradianceProgram_.setFloat("mieStrength", settings_.atmosphereMieStrength);
    atmosphereIrradianceProgram_.setFloat("atmosphereExposure", settings_.atmosphereExposure);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atmosphereLut_.transmittanceTexture);
    atmosphereIrradianceProgram_.setInt("transmittanceTexture", 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, atmosphereLut_.irradianceTexture, 0);
    glViewport(0, 0, atmosphereLut_.irradianceWidth, atmosphereLut_.irradianceHeight);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glViewport(0, 0, atmosphereLut_.scatteringWidth, atmosphereLut_.scatteringHeight);
    auto renderScatteringDelta = [&](GLuint targetTexture, GLuint previousTexture, int order) {
        atmosphereScatteringProgram_.use();
        atmosphereScatteringProgram_.setVec3("rayleighColor", settings_.atmosphereRayleighColor);
        atmosphereScatteringProgram_.setVec3("mieColor", settings_.atmosphereMieColor);
        atmosphereScatteringProgram_.setFloat("atmosphereDensity", settings_.atmosphereDensity);
        atmosphereScatteringProgram_.setFloat("rayleighStrength", settings_.atmosphereRayleighStrength);
        atmosphereScatteringProgram_.setFloat("mieStrength", settings_.atmosphereMieStrength);
        atmosphereScatteringProgram_.setFloat("mieAnisotropy", settings_.atmosphereMieAnisotropy);
        atmosphereScatteringProgram_.setFloat("atmosphereExposure", settings_.atmosphereExposure);
        atmosphereScatteringProgram_.setFloat("layerCount", static_cast<float>(atmosphereLut_.scatteringDepth));
        atmosphereScatteringProgram_.setFloat("viewMuSize", static_cast<float>(atmosphereLut_.scatteringViewMuSize));
        atmosphereScatteringProgram_.setFloat("nuSize", static_cast<float>(atmosphereLut_.scatteringNuSize));
        atmosphereScatteringProgram_.setInt("scatteringOrder", order);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atmosphereLut_.transmittanceTexture);
        atmosphereScatteringProgram_.setInt("transmittanceTexture", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, atmosphereLut_.irradianceTexture);
        atmosphereScatteringProgram_.setInt("irradianceTexture", 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_3D, previousTexture);
        atmosphereScatteringProgram_.setInt("previousScatteringTexture", 2);
        glDisable(GL_BLEND);
        for (int layer = 0; layer < atmosphereLut_.scatteringDepth; ++layer) {
            atmosphereScatteringProgram_.setFloat("layerIndex", static_cast<float>(layer));
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, targetTexture, 0, layer);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    };

    auto accumulateScatteringDelta = [&](GLuint sourceTexture, bool additive) {
        atmosphereAccumulateProgram_.use();
        atmosphereAccumulateProgram_.setFloat("layerCount", static_cast<float>(atmosphereLut_.scatteringDepth));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, sourceTexture);
        atmosphereAccumulateProgram_.setInt("sourceScatteringTexture", 0);
        if (additive) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
        } else {
            glDisable(GL_BLEND);
        }
        for (int layer = 0; layer < atmosphereLut_.scatteringDepth; ++layer) {
            atmosphereAccumulateProgram_.setFloat("layerIndex", static_cast<float>(layer));
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, atmosphereLut_.scatteringTexture, 0, layer);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
    };

    GLuint previousDeltaTexture = atmosphereLut_.deltaScatteringTextureA;
    GLuint currentDeltaTexture = atmosphereLut_.deltaScatteringTextureB;
    renderScatteringDelta(previousDeltaTexture, currentDeltaTexture, 1);
    accumulateScatteringDelta(previousDeltaTexture, false);
    for (int order = 1; order <= atmosphereLut_.scatteringOrderCount; ++order) {
        if (order == 1) {
            continue;
        }
        renderScatteringDelta(currentDeltaTexture, previousDeltaTexture, order);
        accumulateScatteringDelta(currentDeltaTexture, true);
        std::swap(previousDeltaTexture, currentDeltaTexture);
    }
    glDisable(GL_BLEND);

    glBindVertexArray(0);
    glActiveTexture(static_cast<GLenum>(previousActiveTexture));
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
    if (wasDepthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
    if (wasBlendEnabled) {
        glEnable(GL_BLEND);
    }
    atmosphereLut_.cachedSignature = signature;
}

void PlanetRenderer::copyAtmosphereSceneDepth(int framebufferWidth, int framebufferHeight)
{
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        return;
    }

    if (atmosphereSceneDepthTexture_ == 0
        || atmosphereSceneDepthWidth_ != framebufferWidth
        || atmosphereSceneDepthHeight_ != framebufferHeight) {
        if (atmosphereSceneDepthTexture_ != 0) {
            glDeleteTextures(1, &atmosphereSceneDepthTexture_);
            atmosphereSceneDepthTexture_ = 0;
        }

        atmosphereSceneDepthWidth_ = framebufferWidth;
        atmosphereSceneDepthHeight_ = framebufferHeight;
        glGenTextures(1, &atmosphereSceneDepthTexture_);
        glBindTexture(GL_TEXTURE_2D, atmosphereSceneDepthTexture_);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_DEPTH_COMPONENT24,
                     atmosphereSceneDepthWidth_,
                     atmosphereSceneDepthHeight_,
                     0,
                     GL_DEPTH_COMPONENT,
                     GL_FLOAT,
                     nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, atmosphereSceneDepthTexture_);
    }

    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, framebufferWidth, framebufferHeight);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void PlanetRenderer::drawAtmospherePass(const FlyCamera& camera,
                                        const glm::mat4& viewMatrix,
                                        const glm::mat4& projectionMatrix)
{
    PROFILE_SCOPE("Draw Atmosphere Pass");
    if (!settings_.renderAtmosphere || settings_.atmosphereHeight <= 0.001f) {
        return;
    }

    const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean wasDepthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLint previousCullFace = GL_BACK;
    GLint previousDepthFunc = GL_LESS;
    GLint currentViewport[4] = {};
    glGetIntegerv(GL_CULL_FACE_MODE, &previousCullFace);
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);
    glGetIntegerv(GL_VIEWPORT, currentViewport);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    const float atmosphereRadius = settings_.planetRadius + glm::max(settings_.atmosphereHeight, 0.0f);
    const float projectionAspect = projectionMatrix[1][1] != 0.0f
        ? projectionMatrix[1][1] / projectionMatrix[0][0]
        : 1.0f;
    copyAtmosphereSceneDepth(currentViewport[2], currentViewport[3]);

    atmosphereProgram_.use();
    atmosphereProgram_.setMat4("model", modelMatrix_);
    atmosphereProgram_.setMat4("inverseViewProjection", glm::inverse(projectionMatrix * viewMatrix));
    atmosphereProgram_.setVec3("cameraPos", camera.position);
    atmosphereProgram_.setVec3("cameraForward", glm::normalize(camera.front));
    atmosphereProgram_.setVec3("cameraRight", glm::normalize(camera.right));
    atmosphereProgram_.setVec3("cameraUp", glm::normalize(camera.up));
    atmosphereProgram_.setFloat("cameraTanHalfFov", glm::tan(glm::radians(camera.fieldOfView) * 0.5f));
    atmosphereProgram_.setFloat("cameraAspectRatio", projectionAspect);
    atmosphereProgram_.setVec2("framebufferSize", glm::vec2(static_cast<float>(currentViewport[2]),
                                                            static_cast<float>(currentViewport[3])));
    atmosphereProgram_.setVec3("lightDir", lightDirection_);
    atmosphereProgram_.setFloat("planetRadius", settings_.planetRadius);
    atmosphereProgram_.setFloat("atmosphereRadius", atmosphereRadius);
    const float atmosphereThickness = glm::max(atmosphereRadius - settings_.planetRadius, 0.001f);
    atmosphereProgram_.setFloat("surfaceLimbRadius", glm::clamp(seaLevelRadius(),
                                                                settings_.planetRadius,
                                                                atmosphereRadius - atmosphereThickness * 0.20f));
    atmosphereProgram_.setFloat("atmosphereDensity", settings_.atmosphereDensity);
    atmosphereProgram_.setFloat("atmosphereExposure", settings_.atmosphereExposure);
    atmosphereProgram_.setFloat("scatteringViewMuSize", static_cast<float>(atmosphereLut_.scatteringViewMuSize));
    atmosphereProgram_.setFloat("scatteringNuSize", static_cast<float>(atmosphereLut_.scatteringNuSize));
    atmosphereProgram_.setFloat("scatteringHeight", static_cast<float>(atmosphereLut_.scatteringHeight));
    atmosphereProgram_.setFloat("scatteringDepth", static_cast<float>(atmosphereLut_.scatteringDepth));
    atmosphereProgram_.setVec3("mieColor", settings_.atmosphereMieColor);
    atmosphereProgram_.setInt("renderClouds", settings_.renderClouds ? 1 : 0);
    atmosphereProgram_.setFloat("cloudCoverage", settings_.cloudCoverage);
    atmosphereProgram_.setFloat("cloudSharpness", settings_.cloudSharpness);
    atmosphereProgram_.setFloat("cloudScale", settings_.cloudScale);
    atmosphereProgram_.setFloat("cloudSpeed", settings_.cloudSpeed);
    atmosphereProgram_.setFloat("cloudHeight", settings_.cloudHeight);
    atmosphereProgram_.setFloat("cloudThickness", settings_.cloudThickness);
    atmosphereProgram_.setFloat("cloudDensity", settings_.cloudDensity);
    atmosphereProgram_.setFloat("cloudShadowStrength", settings_.cloudShadowStrength);
    atmosphereProgram_.setInt("cloudStepCount", settings_.cloudStepCount);
    atmosphereProgram_.setInt("cloudLightStepCount", settings_.cloudLightStepCount);
    atmosphereProgram_.setFloat("cloudOpacity", settings_.cloudOpacity);
    atmosphereProgram_.setVec3("cloudColor", settings_.cloudColor);
    atmosphereProgram_.setFloat("timeSeconds", currentTimeSeconds_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atmosphereLut_.irradianceTexture);
    atmosphereProgram_.setInt("atmosphereIrradianceTexture", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, atmosphereLut_.scatteringTexture);
    atmosphereProgram_.setInt("atmosphereScatteringTexture", 1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, atmosphereSceneDepthTexture_);
    atmosphereProgram_.setInt("sceneDepthTexture", 2);

    glBindVertexArray(fullscreenVertexArrayObject_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    if (wasDepthTestEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
    glCullFace(static_cast<GLenum>(previousCullFace));
    if (!wasCullEnabled) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
    }
    if (!wasBlendEnabled) {
        glDisable(GL_BLEND);
    }
}

void PlanetRenderer::drawReflectionRefractionPasses(const FlyCamera& camera,
                                                    const glm::mat4& viewMatrix,
                                                    const glm::mat4& projectionMatrix,
                                                    int framebufferWidth,
                                                    int framebufferHeight)
{
    PROFILE_SCOPE("Reflection Refraction Passes");
    lastReflectionUpdated_ = false;
    lastRefractionUpdated_ = false;
    lastReflectionEnabled_ = false;
    lastRefractionEnabled_ = false;

    if (!settings_.renderOcean || visibleOceanPatches_.empty()) {
        oceanReflectionFrameCounter_ = 0;
        oceanRefractionFrameCounter_ = 0;
        oceanReflectionWeight_ = 0.0f;
        oceanRefractionWeight_ = 0.0f;
        return;
    }

    const float seaLevelY = seaLevelRadius();
    const float cameraOceanAltitude = glm::abs(glm::length(camera.position) - seaLevelY);
    auto distanceWeight = [cameraOceanAltitude](float maxAltitude, bool autoLod) {
        if (!autoLod) {
            return 1.0f;
        }

        const float stableMaxAltitude = glm::max(maxAltitude, 1.0f);
        return 1.0f - glm::smoothstep(stableMaxAltitude * 0.75f, stableMaxAltitude, cameraOceanAltitude);
    };

    const bool reflectionUserEnabled = settings_.renderOceanReflectionRefraction && settings_.renderOceanReflection;
    const bool refractionUserEnabled = settings_.renderOceanReflectionRefraction && settings_.renderOceanRefraction;
    const float targetReflectionWeight = reflectionUserEnabled
        ? distanceWeight(settings_.oceanReflectionMaxAltitude, settings_.oceanAutoDistanceLod)
        : 0.0f;
    const float targetRefractionWeight = refractionUserEnabled
        ? distanceWeight(settings_.oceanRefractionMaxAltitude, settings_.oceanAutoDistanceLod)
        : 0.0f;
    const float blendSpeed = 1.0f - std::exp(-currentDeltaSeconds_ * 6.0f);
    oceanReflectionWeight_ = glm::mix(oceanReflectionWeight_, targetReflectionWeight, blendSpeed);
    oceanRefractionWeight_ = glm::mix(oceanRefractionWeight_, targetRefractionWeight, blendSpeed);

    const float reflectionDistanceFade = settings_.oceanAutoDistanceLod
        ? glm::smoothstep(0.0f, glm::max(settings_.oceanReflectionMaxAltitude, 1.0f), cameraOceanAltitude)
        : 0.0f;
    const float rawTargetScale = glm::clamp(
        settings_.oceanReflectionResolutionScale * glm::mix(1.0f, 0.55f, reflectionDistanceFade),
        0.25f,
        1.0f
    );
    const float targetScale = glm::clamp(std::round(rawTargetScale * 8.0f) / 8.0f, 0.25f, 1.0f);
    const int targetWidth = std::max(static_cast<int>(std::round(static_cast<float>(framebufferWidth) * targetScale)), 1);
    const int targetHeight = std::max(static_cast<int>(std::round(static_cast<float>(framebufferHeight) * targetScale)), 1);
    const bool reflectionEnabled = reflectionUserEnabled && oceanReflectionWeight_ > 0.02f;
    const bool refractionEnabled = refractionUserEnabled && oceanRefractionWeight_ > 0.02f;
    const int reflectionStride = std::max(settings_.oceanReflectionFrameStride, 1);
    const int refractionStride = std::max(settings_.oceanRefractionFrameStride, 1);
    const int reflectionWidth = targetWidth;
    const int reflectionHeight = targetHeight;
    const int refractionWidth = targetWidth;
    const int refractionHeight = targetHeight;
    const bool reflectionWasReady = reflectionTarget_.framebufferObject != 0
                                 && reflectionTarget_.width == reflectionWidth
                                 && reflectionTarget_.height == reflectionHeight;
    const bool refractionWasReady = refractionTarget_.framebufferObject != 0
                                  && refractionTarget_.width == refractionWidth
                                  && refractionTarget_.height == refractionHeight;

    reflectionTarget_.create(reflectionWidth, reflectionHeight);
    refractionTarget_.create(refractionWidth, refractionHeight);
    if (reflectionTarget_.framebufferObject == 0 || refractionTarget_.framebufferObject == 0) {
        return;
    }

    if (!settings_.renderTerrain) {
        oceanReflectionWeight_ = 0.0f;
        oceanRefractionWeight_ = 0.0f;
        oceanReflectionFrameCounter_ = 0;
        oceanRefractionFrameCounter_ = 0;

        glBindFramebuffer(GL_FRAMEBUFFER, reflectionTarget_.framebufferObject);
        glViewport(0, 0, reflectionTarget_.width, reflectionTarget_.height);
        glClearColor(settings_.skyColor.r, settings_.skyColor.g, settings_.skyColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, refractionTarget_.framebufferObject);
        glViewport(0, 0, refractionTarget_.width, refractionTarget_.height);
        glClearColor(settings_.oceanDeepColor.r, settings_.oceanDeepColor.g, settings_.oceanDeepColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        return;
    }

    lastReflectionEnabled_ = reflectionEnabled;
    lastRefractionEnabled_ = refractionEnabled;

    const float reflectionPlaneY = seaLevelY;
    const float refractionPlaneY = seaLevelY;

    const bool shouldUpdateReflection = reflectionEnabled
                                      && (!reflectionWasReady || (oceanReflectionFrameCounter_ % reflectionStride) == 0);
    if (shouldUpdateReflection) {
        glBindFramebuffer(GL_FRAMEBUFFER, reflectionTarget_.framebufferObject);
        glViewport(0, 0, reflectionTarget_.width, reflectionTarget_.height);
        glClearColor(settings_.skyColor.r, settings_.skyColor.g, settings_.skyColor.b, 1.0f);
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
        lastReflectionUpdated_ = true;
    } else if (!reflectionEnabled && !reflectionWasReady) {
        glBindFramebuffer(GL_FRAMEBUFFER, reflectionTarget_.framebufferObject);
        glViewport(0, 0, reflectionTarget_.width, reflectionTarget_.height);
        glClearColor(settings_.skyColor.r, settings_.skyColor.g, settings_.skyColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    const int refractionPhase = refractionStride > 1 ? refractionStride / 2 : 0;
    const bool shouldUpdateRefraction = refractionEnabled
                                      && (!refractionWasReady || (oceanRefractionFrameCounter_ % refractionStride) == refractionPhase);
    if (shouldUpdateRefraction) {
        glBindFramebuffer(GL_FRAMEBUFFER, refractionTarget_.framebufferObject);
        glViewport(0, 0, refractionTarget_.width, refractionTarget_.height);
        glClearColor(settings_.skyColor.r, settings_.skyColor.g, settings_.skyColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawTerrainPass(camera, viewMatrix, projectionMatrix, true, refractionPlaneY, true);
        lastRefractionUpdated_ = true;
    } else if (!refractionEnabled && !refractionWasReady) {
        glBindFramebuffer(GL_FRAMEBUFFER, refractionTarget_.framebufferObject);
        glViewport(0, 0, refractionTarget_.width, refractionTarget_.height);
        glClearColor(settings_.oceanDeepColor.r, settings_.oceanDeepColor.g, settings_.oceanDeepColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if (reflectionEnabled) {
        ++oceanReflectionFrameCounter_;
    } else {
        oceanReflectionFrameCounter_ = 0;
    }
    if (refractionEnabled) {
        ++oceanRefractionFrameCounter_;
    } else {
        oceanRefractionFrameCounter_ = 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

void PlanetRenderer::drawWireOverlayPass(const FlyCamera& camera,
                                         const glm::mat4& viewMatrix,
                                         const glm::mat4& projectionMatrix)
{
    PROFILE_SCOPE("Draw Wire Overlay Pass");
    if (settings_.wireMode == PlanetWireMode::None) {
        return;
    }

    if (settings_.wireMode == PlanetWireMode::BakedLod) {
        if (!settings_.renderTerrain || visibleBakedChunks_.empty()) {
            return;
        }

        const glm::mat4 cameraRelativeView = glm::mat4(glm::mat3(viewMatrix));
        bakedChunkBoundsProgram_.use();
        bakedChunkBoundsProgram_.setMat4("model", modelMatrix_);
        bakedChunkBoundsProgram_.setMat4("cameraRelativeView", cameraRelativeView);
        bakedChunkBoundsProgram_.setMat4("projection", projectionMatrix);
        bakedChunkBoundsProgram_.setVec3("cameraPos", camera.position);

        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glLineWidth(1.0f);

        bakedChunkBoundsProgram_.setVec4("lineColor", glm::vec4(0.06f, 0.95f, 0.80f, 0.78f));
        bakedTerrainMesh_.drawWire(visibleBakedChunks_);

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_LINE);
        glLineWidth(1.0f);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glDisable(GL_BLEND);
        return;
    }

    if (settings_.wireMode != PlanetWireMode::Ocean || !settings_.renderOcean || visibleOceanPatches_.empty()) {
        return;
    }

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.heightTexture());
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.normalTexture());
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, fftOcean_.displacementTexture());
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralHeightTexture_);
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralWaterDepthTexture_);

    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    for (const OceanPatch& patch : visibleOceanPatches_) {
        applyCommonUniforms(oceanWireOverlayProgram_, camera, viewMatrix, projectionMatrix, patch);
        oceanWireOverlayProgram_.setFloat("tessMin", settings_.oceanTessellationMin);
        oceanWireOverlayProgram_.setFloat("tessMax", effectiveOceanTessellationMax_);
        oceanWireOverlayProgram_.setFloat("tessMinDist", settings_.oceanTessellationNearDistance);
        oceanWireOverlayProgram_.setFloat("tessMaxDist", settings_.oceanTessellationFarDistance);
        oceanWireOverlayProgram_.setInt("oceanHeightTexture", 3);
        oceanWireOverlayProgram_.setInt("oceanNormalTexture", 4);
        oceanWireOverlayProgram_.setInt("oceanDisplacementTexture", 8);
        oceanWireOverlayProgram_.setInt("proceduralHeightTexture", 9);
        oceanWireOverlayProgram_.setInt("proceduralWaterDepthTexture", 10);
        terrainMesh_.draw();
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    for (const OceanPatch& patch : visibleOceanPatches_) {
        applyCommonUniforms(oceanCoarseGridProgram_, camera, viewMatrix, projectionMatrix, patch);
        oceanCoarseGridProgram_.setFloat("tessMin", settings_.oceanTessellationMin);
        oceanCoarseGridProgram_.setFloat("tessMax", effectiveOceanTessellationMax_);
        oceanCoarseGridProgram_.setFloat("tessMinDist", settings_.oceanTessellationNearDistance);
        oceanCoarseGridProgram_.setFloat("tessMaxDist", settings_.oceanTessellationFarDistance);
        oceanCoarseGridProgram_.setInt("oceanHeightTexture", 3);
        oceanCoarseGridProgram_.setInt("oceanNormalTexture", 4);
        oceanCoarseGridProgram_.setInt("oceanDisplacementTexture", 8);
        oceanCoarseGridProgram_.setInt("proceduralHeightTexture", 9);
        oceanCoarseGridProgram_.setInt("proceduralWaterDepthTexture", 10);
        terrainMesh_.draw();
    }

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    return;
}

void PlanetRenderer::drawFeatureOverlayPass(const FlyCamera& camera,
                                            const glm::mat4& viewMatrix,
                                            const glm::mat4& projectionMatrix)
{
    PROFILE_SCOPE("Draw Feature Overlay Pass");
    if (settings_.featureOverlayMode == TerrainFeatureOverlayMode::None
        || featureSegmentMesh_.vertexCount <= 0
        || proceduralHeightTexture_ == 0) {
        return;
    }

    const GLboolean wasBlendEnabled = glIsEnabled(GL_BLEND);
    GLint previousDepthFunc = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);

    const glm::mat4 cameraRelativeView = glm::mat4(glm::mat3(viewMatrix));

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, proceduralHeightTexture_);

    featureSegmentProgram_.use();
    featureSegmentProgram_.setMat4("model", modelMatrix_);
    featureSegmentProgram_.setMat4("cameraRelativeView", cameraRelativeView);
    featureSegmentProgram_.setMat4("projection", projectionMatrix);
    featureSegmentProgram_.setVec3("cameraPos", camera.position);
    featureSegmentProgram_.setFloat("planetRadius", settings_.planetRadius);
    featureSegmentProgram_.setFloat("heightScale", settings_.terrainHeightScale);
    featureSegmentProgram_.setFloat("seaLevelOffset", settings_.seaLevelOffset);
    featureSegmentProgram_.setFloat("runtimeMountainScale", settings_.runtimeMountainScale);
    featureSegmentProgram_.setFloat("lineLift", glm::max(settings_.terrainHeightScale * 0.012f, 0.08f));
    featureSegmentProgram_.setFloat("proceduralDataTexelSize",
                                    proceduralDataResolution_ > 0 ? 1.0f / static_cast<float>(proceduralDataResolution_) : 0.0f);
    featureSegmentProgram_.setInt("proceduralHeightTexture", 1);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.5f);

    const auto drawType = [&](TerrainFeatureOverlayMode mode, const glm::vec4& color) {
        featureSegmentProgram_.setVec4("lineColor", color);
        featureSegmentMesh_.draw(mode);
    };

    if (settings_.featureOverlayMode == TerrainFeatureOverlayMode::All
        || settings_.featureOverlayMode == TerrainFeatureOverlayMode::Rivers) {
        drawType(TerrainFeatureOverlayMode::Rivers, glm::vec4(0.04f, 0.82f, 1.00f, 0.90f));
    }
    if (settings_.featureOverlayMode == TerrainFeatureOverlayMode::All
        || settings_.featureOverlayMode == TerrainFeatureOverlayMode::Coast) {
        drawType(TerrainFeatureOverlayMode::Coast, glm::vec4(1.00f, 0.78f, 0.32f, 0.86f));
    }
    if (settings_.featureOverlayMode == TerrainFeatureOverlayMode::All
        || settings_.featureOverlayMode == TerrainFeatureOverlayMode::Ridges) {
        drawType(TerrainFeatureOverlayMode::Ridges, glm::vec4(1.00f, 0.26f, 0.72f, 0.88f));
    }
    if (settings_.featureOverlayMode == TerrainFeatureOverlayMode::All
        || settings_.featureOverlayMode == TerrainFeatureOverlayMode::Erosion) {
        drawType(TerrainFeatureOverlayMode::Erosion, glm::vec4(1.00f, 0.26f, 0.08f, 0.84f));
    }

    glLineWidth(1.0f);
    glDepthMask(GL_TRUE);
    glDepthFunc(static_cast<GLenum>(previousDepthFunc));
    if (!wasBlendEnabled) {
        glDisable(GL_BLEND);
    }
}
