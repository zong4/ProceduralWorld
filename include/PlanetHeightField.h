#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

// 高度场层定义。
// 这个头文件把“星球生成数据”抽象成可分层的标量/向量字段，便于后续做
// 局部高精度 tile、特征 LOD 或编辑器工具时复用。
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
    RegionId,
    FeatureMask,
    MeshDensity,
    GeometricError,
    Count
};

// 向量层目前用于存储 biome/material 权重。
enum class PlanetVectorLayer : std::uint8_t {
    BiomeWeightA = 0,
    BiomeWeightB,
    DomainWeight,
    Count
};

// 描述每个标量层的语义：是否影响几何、材质或 LOD。
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
    { PlanetScalarLayer::Moisture, "moisture", 0.0f, false, true, false },
    { PlanetScalarLayer::RegionId, "regionId", 0.0f, false, true, true },
    { PlanetScalarLayer::FeatureMask, "featureMask", 0.0f, false, true, true },
    { PlanetScalarLayer::MeshDensity, "meshDensity", 0.0f, false, false, true },
    { PlanetScalarLayer::GeometricError, "geometricError", 0.0f, false, false, true }
}};

// 单点采样结果，聚合所有当前项目会用到的地形/水文/材质字段。
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
    float regionId = 0.0f;
    float featureMask = 0.0f;
    float meshDensity = 0.0f;
    float geometricError = 0.0f;
    glm::vec4 biomeWeightA = glm::vec4(0.0f);
    glm::vec4 biomeWeightB = glm::vec4(0.0f);
    glm::vec4 domainWeight = glm::vec4(0.0f);
};

// 单个 cube face 的完整层数据。
struct PlanetHeightFieldFace {
    int resolution = 0;
    std::array<std::vector<float>, static_cast<std::size_t>(PlanetScalarLayer::Count)> scalarLayers;
    std::array<std::vector<glm::vec4>, static_cast<std::size_t>(PlanetVectorLayer::Count)> vectorLayers;
};

// 整颗星球的六面体高度场快照。
struct PlanetGlobalHeightField {
    int faceResolution = 0;
    std::array<PlanetHeightFieldFace, 6> faces;
    float minHeight = 0.0f;
    float maxHeight = 0.0f;
    float maxWaterDepth = 0.0f;
    float waterCoverage = 0.0f;
    float shoreCoverage = 0.0f;
};

// 局部 tile 的标识，后续可用于更高 LOD 的局部地貌增量。
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

// tile 在 cube face UV 空间中的范围。
struct PlanetHeightFieldTileBounds {
    int face = 0;
    glm::vec2 uvMin = glm::vec2(0.0f);
    glm::vec2 uvSize = glm::vec2(1.0f);
};

// 局部高度场 tile：存储相对全局高度场的增量和局部材质权重。
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

// 局部高度场生成配置。当前项目主要使用全局高度场，这组结构为扩展预留。
struct PlanetLocalHeightFieldConfig {
    int tileResolution = 128;
    int tileRadius = 1;
    int minLod = 5;
    int maxLod = 9;
    float heightStrength = 1.0f;
    float blendBorderTexels = 8.0f;
};

// 请求某类地貌特征需要的 LOD 偏置。
// 例如海岸、河道或 biome 边界附近可以要求更高细分。
struct PlanetFeatureLodRequest {
    int minimumDepth = 0;
    float splitBias = 1.0f;
    float shoreWeight = 0.0f;
    float slopeWeight = 0.0f;
    float channelWeight = 0.0f;
    float wearWeight = 0.0f;
    float biomeBoundaryWeight = 0.0f;
};

// 最终高度组合结果：全局高度 + 局部增量 + 运行时微细节。
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
