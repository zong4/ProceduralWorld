#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

enum class PlanetScalarLayer : std::uint8_t {
    Height = 0,
    WaterDepth,
    ShoreMask,
    ErosionMask,
    ChannelMask,
    FlowMask,
    WearMask,
    DepositionMask,
    Temperature,
    Moisture,
    Count
};

enum class PlanetVectorLayer : std::uint8_t {
    BiomeWeightA = 0,
    BiomeWeightB,
    Count
};

struct PlanetHeightFieldLayerInfo {
    PlanetScalarLayer layer = PlanetScalarLayer::Height;
    const char* name = "height";
    float defaultValue = 0.0f;
    bool affectsGeometry = false;
    bool affectsMaterial = true;
    bool affectsLod = false;
};

inline constexpr std::array<PlanetHeightFieldLayerInfo, static_cast<std::size_t>(PlanetScalarLayer::Count)> kPlanetScalarLayerInfo = {{
    { PlanetScalarLayer::Height, "height", 0.0f, true, true, true },
    { PlanetScalarLayer::WaterDepth, "waterDepth", 0.0f, false, true, true },
    { PlanetScalarLayer::ShoreMask, "shoreMask", 0.0f, false, true, true },
    { PlanetScalarLayer::ErosionMask, "erosionMask", 0.0f, false, true, true },
    { PlanetScalarLayer::ChannelMask, "channelMask", 0.0f, false, true, true },
    { PlanetScalarLayer::FlowMask, "flowMask", 0.0f, false, true, true },
    { PlanetScalarLayer::WearMask, "wearMask", 0.0f, false, true, true },
    { PlanetScalarLayer::DepositionMask, "depositionMask", 0.0f, false, true, true },
    { PlanetScalarLayer::Temperature, "temperature", 0.0f, false, true, false },
    { PlanetScalarLayer::Moisture, "moisture", 0.0f, false, true, false }
}};

struct PlanetHeightFieldSample {
    float height = 0.0f;
    float waterDepth = 0.0f;
    float shoreMask = 0.0f;
    float erosionMask = 0.0f;
    float channelMask = 0.0f;
    float flowMask = 0.0f;
    float wearMask = 0.0f;
    float depositionMask = 0.0f;
    float temperature = 0.0f;
    float moisture = 0.0f;
    glm::vec4 biomeWeightA = glm::vec4(0.0f);
    glm::vec4 biomeWeightB = glm::vec4(0.0f);
};

struct PlanetHeightFieldFace {
    int resolution = 0;
    std::array<std::vector<float>, static_cast<std::size_t>(PlanetScalarLayer::Count)> scalarLayers;
    std::array<std::vector<glm::vec4>, static_cast<std::size_t>(PlanetVectorLayer::Count)> vectorLayers;
};

struct PlanetGlobalHeightField {
    int faceResolution = 0;
    std::array<PlanetHeightFieldFace, 6> faces;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
    float maxWaterDepth = 0.0f;
    float waterCoverage = 0.0f;
    float shoreCoverage = 0.0f;
};

struct PlanetHeightFieldTileId {
    int face = 0;
    int lod = 0;
    int x = 0;
    int y = 0;

    bool operator==(const PlanetHeightFieldTileId& other) const
    {
        return face == other.face && lod == other.lod && x == other.x && y == other.y;
    }
};

struct PlanetHeightFieldTileBounds {
    int face = 0;
    glm::vec2 uvMin = glm::vec2(0.0f);
    glm::vec2 uvSize = glm::vec2(1.0f);
};

struct PlanetLocalHeightFieldTile {
    PlanetHeightFieldTileId id;
    PlanetHeightFieldTileBounds bounds;
    int resolution = 0;
    float worldTexelSize = 0.0f;
    bool generated = false;
    bool residentOnGpu = false;

    std::vector<float> heightDelta;
    std::vector<float> valleyMask;
    std::vector<float> channelMask;
    std::vector<float> flowMask;
    std::vector<float> wearMask;
    std::vector<float> depositionMask;
    std::vector<glm::vec4> materialWeightA;
    std::vector<glm::vec4> materialWeightB;
};

struct PlanetLocalHeightFieldConfig {
    int tileResolution = 128;
    int tileRadius = 1;
    int minLod = 5;
    int maxLod = 9;
    float heightStrength = 1.0f;
    float blendBorderTexels = 8.0f;
};

struct PlanetFeatureLodRequest {
    int minimumDepth = 0;
    float splitBias = 1.0f;
    float shoreWeight = 0.0f;
    float slopeWeight = 0.0f;
    float channelWeight = 0.0f;
    float wearWeight = 0.0f;
    float biomeBoundaryWeight = 0.0f;
};

struct PlanetHeightComposition {
    float globalHeight = 0.0f;
    float localHeightDelta = 0.0f;
    float localWeight = 0.0f;
    float microDetail = 0.0f;

    float finalHeight() const
    {
        return globalHeight + localHeightDelta * localWeight + microDetail;
    }
};
