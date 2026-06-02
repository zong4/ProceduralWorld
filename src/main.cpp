#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <cstdlib>
#include <unordered_map>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "FlyCamera.h"
#include "Instumentor/InstrumentationTimer.hpp"
#include "Instumentor/Instrumentor.hpp"
#include "PlanetProceduralData.h"
#include "PlanetRenderer.h"

#ifdef _WIN32
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace
{
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr float kLockedCameraFov = 50.0f;
constexpr float kOrbitAngularSpeedDegrees = 42.0f;
constexpr const char* kSessionFilePath = "config/last_session.ini";
constexpr const char* kProceduralCacheFilePath = "config/last_procedural_cache.bin";
constexpr const char* kProfileTraceFilePath = "profile_trace.json";
constexpr float kReferencePlanetRadius = 200.0f;
constexpr int kGenerationModuleCount = static_cast<int>(PlanetProceduralData::GenerationModule::Count);

// 应用级工作流�?
// ProceduralSetup 负责调生成参数；Generating 后台烘焙数据；Render 实时渲染�?
enum class WorkflowStage {
    ProceduralSetup,
    Generating,
    Render
};

// 主程序的所有可变状态集中在这里，避�?GLFW/ImGui 回调之间传递大量全局变量�?
struct ApplicationState {
    FlyCamera camera{glm::vec3(0.0f, 90.0f, 420.0f)};
    PlanetRenderer renderer;
    PlanetRenderSettings proceduralSettings;
    PlanetRenderSettings renderSettings;
    PlanetProceduralData generatedPlanet;
    WorkflowStage workflowStage = WorkflowStage::ProceduralSetup;
    int generationFaceResolution = 256;
    float generationTimer = 0.0f;
    float generationDuration = 0.55f;
    PlanetRenderSettings pendingGenerationSettings;
    std::atomic<int> generationCompletedSteps{0};
    std::atomic<int> generationTotalSteps{1};
    std::atomic<int> generationActiveModule{0};
    std::array<std::atomic<int>, kGenerationModuleCount> generationModuleCompletedSteps{};
    std::array<std::atomic<int>, kGenerationModuleCount> generationModuleTotalSteps{};
    std::mutex generationStatusMutex;
    std::string generationStatusText = "Preparing generation";
    std::future<std::unique_ptr<PlanetProceduralData>> generationFuture;
    bool hasGeneratedPlanet = false;
    bool firstRightMouseSample = true;
    bool firstLeftMouseSample = true;
    float lastRightMouseX = kWindowWidth * 0.5f;
    float lastRightMouseY = kWindowHeight * 0.5f;
    float lastLeftMouseX = kWindowWidth * 0.5f;
    float lastLeftMouseY = kWindowHeight * 0.5f;
    float deltaSeconds = 0.0f;
    float previousFrameTime = 0.0f;
    bool showDebugPanel = true;
    bool showPerformancePanel = false;
    bool showProceduralTerrainFeature = false;
    bool showProceduralErosionFeature = false;
    bool showProceduralMaterialFeature = false;
    bool showRenderModeFeature = false;
    bool showVisibilityFeature = false;
    bool showTerrainRuntimeFeature = false;
    bool showLightingFeature = false;
    bool showRiverFeature = false;
    bool showAtmosphereFeature = false;
    bool showOceanColorFeature = false;
    bool showOceanWaveFeature = false;
    bool showOceanMaterialFeature = false;
    bool showOceanReflectionFeature = false;
    bool showAdvancedRenderFeature = false;
    bool showCameraFeature = false;
    float planetYawDegrees = 0.0f;
    float planetPitchDegrees = 0.0f;
    float cameraOrbitYawDegrees = 0.0f;
    float cameraOrbitPitchDegrees = 12.0f;
    float cameraOrbitDistance = 420.0f;
    std::string sessionMessage;
};

ApplicationState* getState(GLFWwindow* window)
{
    return static_cast<ApplicationState*>(glfwGetWindowUserPointer(window));
}

void collapseFeaturePanels(ApplicationState& state)
{
    state.showProceduralTerrainFeature = false;
    state.showProceduralErosionFeature = false;
    state.showProceduralMaterialFeature = false;
    state.showRenderModeFeature = false;
    state.showVisibilityFeature = false;
    state.showTerrainRuntimeFeature = false;
    state.showLightingFeature = false;
    state.showRiverFeature = false;
    state.showAtmosphereFeature = false;
    state.showOceanColorFeature = false;
    state.showOceanWaveFeature = false;
    state.showOceanMaterialFeature = false;
    state.showOceanReflectionFeature = false;
    state.showAdvancedRenderFeature = false;
    state.showCameraFeature = false;
}

void selectFeaturePanel(ApplicationState& state, bool& panelOpen)
{
    const bool openTarget = !panelOpen;
    collapseFeaturePanels(state);
    panelOpen = openTarget;
}

// 复制会影响“重新生成星球”或生成后地表显示的参数�?
// 海水反射、大气、相机、LOD 等纯运行时参数保留当�?renderSettings�?
void copyProceduralSettings(PlanetRenderSettings& destination, const PlanetRenderSettings& source)
{
    destination.planetRadius = source.planetRadius;
    destination.seaLevelOffset = source.seaLevelOffset;
    destination.terrainHeightScale = source.terrainHeightScale;
    destination.terrainNoiseScale = source.terrainNoiseScale;
    destination.mountainMaskStrength = source.mountainMaskStrength;
    destination.mountainMaskScale = source.mountainMaskScale;
    destination.mountainRidgeSharpness = source.mountainRidgeSharpness;
    destination.erosionIterations = 0;
    destination.erosionStrength = 0.0f;
    destination.erosionTalus = source.erosionTalus;
    destination.erosionSediment = source.erosionSediment;
    destination.erosionThermalStrength = 0.0f;
    destination.terrainLowlandColor = source.terrainLowlandColor;
    destination.terrainForestColor = source.terrainForestColor;
    destination.terrainDesertColor = source.terrainDesertColor;
    destination.terrainRockColor = source.terrainRockColor;
    destination.terrainBeachColor = source.terrainBeachColor;
    destination.terrainSnowColor = source.terrainSnowColor;
    destination.terrainBeachWidth = source.terrainBeachWidth;
    destination.terrainRockSlopeStart = source.terrainRockSlopeStart;
    destination.terrainRockSlopeEnd = source.terrainRockSlopeEnd;
    destination.terrainSnowStart = source.terrainSnowStart;
    destination.terrainSnowEnd = source.terrainSnowEnd;
    destination.terrainMaterialNoiseScale = source.terrainMaterialNoiseScale;
    destination.terrainMaterialNoiseStrength = 0.0f;
    destination.renderRivers = source.renderRivers;
    destination.riverVisibility = source.riverVisibility;
    destination.riverWidth = source.riverWidth;
    destination.riverShine = source.riverShine;
    destination.riverRefractionStrength = source.riverRefractionStrength;
    destination.riverColor = source.riverColor;
}

float minCameraOrbitDistance(const PlanetRenderSettings& settings)
{
    return settings.planetRadius + glm::max(settings.terrainHeightScale, 1.0f) + 4.0f;
}

// 相机轨道半径随星球半径缩放，保证小星�?大星球都能被观察�?
float maxCameraOrbitDistance(const PlanetRenderSettings& settings)
{
    return glm::max(settings.planetRadius * 8.0f, minCameraOrbitDistance(settings) + 10.0f);
}

float wrapDegrees(float degrees)
{
    degrees = std::fmod(degrees, 360.0f);
    if (degrees < 0.0f) {
        degrees += 360.0f;
    }
    return degrees;
}

glm::vec3 rotateVectorAroundAxis(const glm::vec3& value, const glm::vec3& axis, float degrees)
{
    return glm::vec3(glm::rotate(glm::mat4(1.0f), glm::radians(degrees), glm::normalize(axis)) * glm::vec4(value, 0.0f));
}

void updateOrbitMetadata(ApplicationState& state)
{
    state.cameraOrbitDistance = glm::length(state.camera.position);
    if (glm::length(state.camera.front) <= 0.001f) {
        return;
    }

    const glm::vec3 direction = glm::normalize(state.camera.front);
    state.cameraOrbitPitchDegrees = glm::degrees(std::asin(glm::clamp(direction.y, -1.0f, 1.0f)));
    state.cameraOrbitYawDegrees = wrapDegrees(glm::degrees(std::atan2(direction.x, direction.z)));
}

glm::vec3 orbitDirectionFromAngles(float yawDegrees, float pitchDegrees)
{
    const float yawRadians = glm::radians(yawDegrees);
    const float pitchRadians = glm::radians(glm::clamp(pitchDegrees, -85.0f, 85.0f));
    const float cosPitch = std::cos(pitchRadians);
    return glm::normalize(glm::vec3(
        std::sin(yawRadians) * cosPitch,
        std::sin(pitchRadians),
        std::cos(yawRadians) * cosPitch
    ));
}

// 将相机朝向锁定到星球中心，同时尽量保留用户期望的上方向�?
void orientCameraToPlanet(ApplicationState& state, const glm::vec3& preferredUp)
{
    glm::vec3 radialDirection = glm::normalize(state.camera.position);
    state.camera.front = -radialDirection;

    glm::vec3 upCandidate = preferredUp - state.camera.front * glm::dot(preferredUp, state.camera.front);
    if (glm::length(upCandidate) < 0.0001f) {
        upCandidate = glm::abs(state.camera.front.y) < 0.95f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
        upCandidate -= state.camera.front * glm::dot(upCandidate, state.camera.front);
    }

    const glm::vec3 orbitUp = glm::normalize(upCandidate);
    state.camera.right = glm::normalize(glm::cross(state.camera.front, orbitUp));
    state.camera.up = glm::normalize(glm::cross(state.camera.right, state.camera.front));
}

// 统一维护轨道相机约束：FOV 固定、距离裁剪、朝向星球中心�?
void updateOrbitCamera(ApplicationState& state, const PlanetRenderSettings& settings)
{
    (void)settings;
    state.camera.fieldOfView = kLockedCameraFov;
}

// 从已有相机位置恢复轨道相机元数据，常用于加载 session 或生成完成后重定位�?
void setOrbitFromCameraPosition(ApplicationState& state, const PlanetRenderSettings& settings)
{
    state.cameraOrbitDistance = glm::clamp(
        glm::length(state.camera.position),
        minCameraOrbitDistance(settings),
        maxCameraOrbitDistance(settings)
    );
    if (glm::length(state.camera.position) <= 0.001f) {
        state.camera.position = glm::vec3(0.0f, 0.0f, state.cameraOrbitDistance);
    } else {
        state.camera.position = glm::normalize(state.camera.position) * state.cameraOrbitDistance;
    }
    orientCameraToPlanet(state, glm::vec3(0.0f, 1.0f, 0.0f));
    updateOrbitMetadata(state);
}

using SessionValues = std::unordered_map<std::string, std::string>;

// session 文件使用简�?key=value 格式，下面这�?helper 负责读写和类型转换�?
std::string trimString(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}
std::string sessionKey(const std::string& prefix, const char* name)
{
    return prefix + "." + name;
}

void writeFloat(std::ostream& out, const std::string& key, float value)
{
    out << key << "=" << value << "\n";
}

void writeInt(std::ostream& out, const std::string& key, int value)
{
    out << key << "=" << value << "\n";
}

void writeBool(std::ostream& out, const std::string& key, bool value)
{
    out << key << "=" << (value ? 1 : 0) << "\n";
}

void writeVec3(std::ostream& out, const std::string& key, const glm::vec3& value)
{
    out << key << "=" << value.x << " " << value.y << " " << value.z << "\n";
}

bool readSessionFile(SessionValues& values, const char* path)
{
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line = line.substr(0, comment);
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = trimString(line.substr(0, equals));
        const std::string value = trimString(line.substr(equals + 1));
        if (!key.empty()) {
            values[key] = value;
        }
    }

    return true;
}

bool readFloat(const SessionValues& values, const std::string& key, float& outValue)
{
    const auto it = values.find(key);
    if (it == values.end()) {
        return false;
    }

    try {
        outValue = std::stof(it->second);
        return true;
    } catch (...) {
        return false;
    }
}

bool readInt(const SessionValues& values, const std::string& key, int& outValue)
{
    const auto it = values.find(key);
    if (it == values.end()) {
        return false;
    }

    try {
        outValue = std::stoi(it->second);
        return true;
    } catch (...) {
        return false;
    }
}

bool readBool(const SessionValues& values, const std::string& key, bool& outValue)
{
    int parsed = 0;
    if (readInt(values, key, parsed)) {
        outValue = parsed != 0;
        return true;
    }

    const auto it = values.find(key);
    if (it == values.end()) {
        return false;
    }

    const std::string value = trimString(it->second);
    if (value == "true" || value == "True") {
        outValue = true;
        return true;
    }
    if (value == "false" || value == "False") {
        outValue = false;
        return true;
    }
    return false;
}

bool readVec3(const SessionValues& values, const std::string& key, glm::vec3& outValue)
{
    const auto it = values.find(key);
    if (it == values.end()) {
        return false;
    }

    std::istringstream stream(it->second);
    glm::vec3 parsed(0.0f);
    if (stream >> parsed.x >> parsed.y >> parsed.z) {
        outValue = parsed;
        return true;
    }
    return false;
}

// 用宏列出所有需要保�?加载的设置字段，避免 writeSettings/readSettings 漏字段�?
#define PLANET_SETTING_FLOAT_FIELDS(X) \
    X(planetRadius) \
    X(seaLevelOffset) \
    X(oceanTessellationMax) \
    X(oceanTessellationMin) \
    X(oceanTessellationNearDistance) \
    X(oceanTessellationFarDistance) \
    X(terrainHeightScale) \
    X(terrainNoiseScale) \
    X(mountainMaskStrength) \
    X(mountainMaskScale) \
    X(mountainRidgeSharpness) \
    X(erosionStrength) \
    X(erosionTalus) \
    X(erosionSediment) \
    X(erosionThermalStrength) \
    X(terrainBeachWidth) \
    X(terrainRockSlopeStart) \
    X(terrainRockSlopeEnd) \
    X(terrainSnowStart) \
    X(terrainSnowEnd) \
    X(terrainMaterialNoiseScale) \
    X(terrainMaterialNoiseStrength) \
    X(riverVisibility) \
    X(riverWidth) \
    X(riverShine) \
    X(riverRefractionStrength) \
    X(coarseGridLineWidth) \
    X(fogDensity) \
    X(atmosphereHeight) \
    X(atmosphereDensity) \
    X(atmosphereRayleighStrength) \
    X(atmosphereMieStrength) \
    X(atmosphereMieAnisotropy) \
    X(atmosphereExposure) \
    X(cloudCoverage) \
    X(cloudSharpness) \
    X(cloudScale) \
    X(cloudSpeed) \
    X(cloudRotationSpeed) \
    X(cloudHeight) \
    X(cloudOpacity) \
    X(cameraNearPlane) \
    X(cameraFarPlane) \
    X(oceanAlpha) \
    X(oceanShallowAlpha) \
    X(oceanDeepAlpha) \
    X(oceanFresnelStrength) \
    X(oceanDistortionStrength) \
    X(oceanDepthRange) \
    X(oceanShallowDepthRange) \
    X(oceanDepthScale) \
    X(oceanTintStrength) \
    X(oceanWaveAmplitude) \
    X(oceanChoppiness) \
    X(oceanWaveTileScale) \
    X(oceanWaveNormalStrength) \
    X(oceanDetailNormalStrength) \
    X(oceanDetailNormalScale) \
    X(oceanDetailFadeDistance) \
    X(oceanSpecularStrength) \
    X(oceanSpecularSharpness) \
    X(oceanRoughness) \
    X(oceanSSSStrength) \
    X(oceanSSSPower) \
    X(oceanShoreBlendWidth) \
    X(oceanReflectionResolutionScale) \
    X(oceanReflectionMaxAltitude) \
    X(oceanRefractionMaxAltitude)

#define PLANET_SETTING_INT_FIELDS(X) \
    X(erosionIterations) \
    X(oceanReflectionFrameStride) \
    X(oceanRefractionFrameStride) \
    X(oceanFftCascadeCount) \
    X(oceanFftFrameStride)

#define PLANET_SETTING_BOOL_FIELDS(X) \
    X(renderAtmosphere) \
    X(renderClouds) \
    X(renderOceanReflectionRefraction) \
    X(renderOceanReflection) \
    X(renderOceanRefraction) \
    X(oceanAutoDistanceLod) \
    X(renderOceanWaves) \
    X(renderOceanMaterial) \
    X(renderRivers) \
    X(renderTerrain) \
    X(renderOcean)

#define PLANET_SETTING_VEC3_FIELDS(X) \
    X(terrainLowlandColor) \
    X(terrainForestColor) \
    X(terrainDesertColor) \
    X(terrainRockColor) \
    X(terrainBeachColor) \
    X(terrainSnowColor) \
    X(riverColor) \
    X(skyColor) \
    X(atmosphereRayleighColor) \
    X(atmosphereMieColor) \
    X(cloudColor) \
    X(oceanShallowColor) \
    X(oceanDeepColor) \
    X(oceanSSSColor)

void writeSettings(std::ostream& out, const std::string& prefix, const PlanetRenderSettings& settings)
{
#define WRITE_FLOAT_FIELD(name) writeFloat(out, sessionKey(prefix, #name), settings.name);
#define WRITE_INT_FIELD(name) writeInt(out, sessionKey(prefix, #name), settings.name);
#define WRITE_BOOL_FIELD(name) writeBool(out, sessionKey(prefix, #name), settings.name);
#define WRITE_VEC3_FIELD(name) writeVec3(out, sessionKey(prefix, #name), settings.name);
    PLANET_SETTING_FLOAT_FIELDS(WRITE_FLOAT_FIELD)
    PLANET_SETTING_INT_FIELDS(WRITE_INT_FIELD)
    PLANET_SETTING_BOOL_FIELDS(WRITE_BOOL_FIELD)
    PLANET_SETTING_VEC3_FIELDS(WRITE_VEC3_FIELD)
#undef WRITE_FLOAT_FIELD
#undef WRITE_INT_FIELD
#undef WRITE_BOOL_FIELD
#undef WRITE_VEC3_FIELD

    writeInt(out, sessionKey(prefix, "renderMode"), static_cast<int>(settings.renderMode));
    writeInt(out, sessionKey(prefix, "wireMode"), static_cast<int>(settings.wireMode));
}

// 读取 session 后会对部分字段做 clamp，防止旧配置或手改文件产生非法范围�?
void readSettings(const SessionValues& values, const std::string& prefix, PlanetRenderSettings& settings)
{
#define READ_FLOAT_FIELD(name) readFloat(values, sessionKey(prefix, #name), settings.name);
#define READ_INT_FIELD(name) readInt(values, sessionKey(prefix, #name), settings.name);
#define READ_BOOL_FIELD(name) readBool(values, sessionKey(prefix, #name), settings.name);
#define READ_VEC3_FIELD(name) readVec3(values, sessionKey(prefix, #name), settings.name);
    PLANET_SETTING_FLOAT_FIELDS(READ_FLOAT_FIELD)
    PLANET_SETTING_INT_FIELDS(READ_INT_FIELD)
    PLANET_SETTING_BOOL_FIELDS(READ_BOOL_FIELD)
    PLANET_SETTING_VEC3_FIELDS(READ_VEC3_FIELD)
#undef READ_FLOAT_FIELD
#undef READ_INT_FIELD
#undef READ_BOOL_FIELD
#undef READ_VEC3_FIELD

    int renderMode = static_cast<int>(settings.renderMode);
    if (readInt(values, sessionKey(prefix, "renderMode"), renderMode)) {
        settings.renderMode = static_cast<PlanetRenderMode>(glm::clamp(renderMode, 0, 3));
    }

    int wireMode = static_cast<int>(settings.wireMode);
    if (readInt(values, sessionKey(prefix, "wireMode"), wireMode)) {
        settings.wireMode = static_cast<PlanetWireMode>(glm::clamp(wireMode, 0, 4));
    }

    settings.featureOverlayMode = TerrainFeatureOverlayMode::None;

    settings.erosionIterations = 0;
    settings.erosionStrength = 0.0f;
    settings.erosionThermalStrength = 0.0f;
    settings.terrainMaterialNoiseStrength = 0.0f;
    settings.oceanFftCascadeCount = glm::clamp(settings.oceanFftCascadeCount, 1, 3);
    settings.oceanFftFrameStride = glm::max(settings.oceanFftFrameStride, 1);
    settings.oceanReflectionFrameStride = glm::max(settings.oceanReflectionFrameStride, 1);
    settings.oceanRefractionFrameStride = glm::max(settings.oceanRefractionFrameStride, 1);
}

// 保存 UI/session 参数；如果当前已有生成数据，同时保存一份二进制缓存�?
bool saveSession(ApplicationState& state, const char* path = kSessionFilePath)
{
    if (state.workflowStage == WorkflowStage::Render) {
        state.renderSettings = state.renderer.settings();
    }

    std::filesystem::path sessionPath(path);
    if (!sessionPath.parent_path().empty()) {
        std::filesystem::create_directories(sessionPath.parent_path());
    }

    std::ofstream file(path);
    if (!file) {
        state.sessionMessage = "Save failed: could not open session file.";
        return false;
    }

    file << std::setprecision(9);
    file << "# ProceduralWorld local session\n";
    file << "version=1\n";
    writeInt(file, "generationFaceResolution", state.generationFaceResolution);
    writeFloat(file, "planetYawDegrees", state.planetYawDegrees);
    writeFloat(file, "planetPitchDegrees", state.planetPitchDegrees);
    writeVec3(file, "cameraPosition", state.camera.position);
    writeVec3(file, "cameraFront", state.camera.front);
    writeVec3(file, "cameraUp", state.camera.up);    writeFloat(file, "cameraOrbitDistance", state.cameraOrbitDistance);
    writeFloat(file, "cameraMouseSensitivity", state.camera.mouseSensitivity);
    writeBool(file, "showPerformancePanel", state.showPerformancePanel);
    writeBool(file, "showProceduralTerrainFeature", state.showProceduralTerrainFeature);
    writeBool(file, "showProceduralErosionFeature", state.showProceduralErosionFeature);
    writeBool(file, "showProceduralMaterialFeature", state.showProceduralMaterialFeature);
    writeBool(file, "showRenderModeFeature", state.showRenderModeFeature);
    writeBool(file, "showVisibilityFeature", state.showVisibilityFeature);
    writeBool(file, "showTerrainRuntimeFeature", state.showTerrainRuntimeFeature);
    writeBool(file, "showLightingFeature", state.showLightingFeature);
    writeBool(file, "showRiverFeature", state.showRiverFeature);
    writeBool(file, "showAtmosphereFeature", state.showAtmosphereFeature);
    writeBool(file, "showOceanColorFeature", state.showOceanColorFeature);
    writeBool(file, "showOceanWaveFeature", state.showOceanWaveFeature);
    writeBool(file, "showOceanMaterialFeature", state.showOceanMaterialFeature);
    writeBool(file, "showOceanReflectionFeature", state.showOceanReflectionFeature);
    writeBool(file, "showAdvancedRenderFeature", state.showAdvancedRenderFeature);
    writeBool(file, "showCameraFeature", state.showCameraFeature);
    writeSettings(file, "procedural", state.proceduralSettings);
    writeSettings(file, "render", state.renderSettings);

    bool savedCache = false;
    if (state.generatedPlanet.isGenerated()) {
        savedCache = state.generatedPlanet.saveCache(kProceduralCacheFilePath);
    }

    state.sessionMessage = savedCache
        ? std::string("Saved session and cache: ") + path
        : std::string("Saved session: ") + path;
    return true;
}

// 读取 session，并尝试恢复上次生成的星球缓存�?
// 缓存加载成功后会立即上传�?renderer，用户可以直接进�?Render 阶段�?
bool loadSession(ApplicationState& state, const char* path = kSessionFilePath, bool reportMissing = true)
{
    SessionValues values;
    if (!readSessionFile(values, path)) {
        if (reportMissing) {
            state.sessionMessage = std::string("No saved session: ") + path;
        }
        return false;
    }

    readSettings(values, "procedural", state.proceduralSettings);
    readSettings(values, "render", state.renderSettings);
    readInt(values, "generationFaceResolution", state.generationFaceResolution);
    state.generationFaceResolution = glm::clamp(state.generationFaceResolution, 256, 512);
    readFloat(values, "planetYawDegrees", state.planetYawDegrees);
    readFloat(values, "planetPitchDegrees", state.planetPitchDegrees);
    readVec3(values, "cameraPosition", state.camera.position);
    const bool loadedCameraFront = readVec3(values, "cameraFront", state.camera.front);
    readVec3(values, "cameraUp", state.camera.up);    readFloat(values, "cameraOrbitDistance", state.cameraOrbitDistance);
    readFloat(values, "cameraMouseSensitivity", state.camera.mouseSensitivity);
    readBool(values, "showPerformancePanel", state.showPerformancePanel);
    readBool(values, "showProceduralTerrainFeature", state.showProceduralTerrainFeature);
    readBool(values, "showProceduralErosionFeature", state.showProceduralErosionFeature);
    readBool(values, "showProceduralMaterialFeature", state.showProceduralMaterialFeature);
    readBool(values, "showRenderModeFeature", state.showRenderModeFeature);
    readBool(values, "showVisibilityFeature", state.showVisibilityFeature);
    readBool(values, "showTerrainRuntimeFeature", state.showTerrainRuntimeFeature);
    readBool(values, "showLightingFeature", state.showLightingFeature);
    readBool(values, "showRiverFeature", state.showRiverFeature);
    readBool(values, "showAtmosphereFeature", state.showAtmosphereFeature);
    readBool(values, "showOceanColorFeature", state.showOceanColorFeature);
    readBool(values, "showOceanWaveFeature", state.showOceanWaveFeature);
    readBool(values, "showOceanMaterialFeature", state.showOceanMaterialFeature);
    readBool(values, "showOceanReflectionFeature", state.showOceanReflectionFeature);
    readBool(values, "showAdvancedRenderFeature", state.showAdvancedRenderFeature);
    readBool(values, "showCameraFeature", state.showCameraFeature);
    collapseFeaturePanels(state);

    state.renderer.settings() = state.renderSettings;
    state.renderer.setPlanetRotation(state.planetYawDegrees, state.planetPitchDegrees);

    const bool loadedCache = state.generatedPlanet.loadCache(kProceduralCacheFilePath, state.renderSettings);
    if (loadedCache) {
        state.renderer.setProceduralData(state.generatedPlanet);
        state.hasGeneratedPlanet = true;
        state.workflowStage = WorkflowStage::Render;
    } else {
        state.generatedPlanet.clear();
        state.hasGeneratedPlanet = false;
        state.workflowStage = WorkflowStage::ProceduralSetup;
        state.showDebugPanel = true;
    }

    const PlanetRenderSettings& activeSettings = state.workflowStage == WorkflowStage::Render
        ? state.renderer.settings()
        : state.renderSettings;
    if (glm::length(state.camera.position) <= 0.001f) {
        state.camera.position = glm::vec3(0.0f, 0.0f, activeSettings.planetRadius * 2.1f);
    }
    if (loadedCameraFront && glm::length(state.camera.front) > 0.001f) {
        state.camera.front = glm::normalize(state.camera.front);
        glm::vec3 upCandidate = state.camera.up - state.camera.front * glm::dot(state.camera.up, state.camera.front);
        if (glm::length(upCandidate) < 0.001f) {
            upCandidate = glm::vec3(0.0f, 1.0f, 0.0f) - state.camera.front * glm::dot(glm::vec3(0.0f, 1.0f, 0.0f), state.camera.front);
        }
        if (glm::length(upCandidate) < 0.001f) {
            upCandidate = glm::vec3(1.0f, 0.0f, 0.0f);
        }
        state.camera.up = glm::normalize(upCandidate);
        state.camera.right = glm::normalize(glm::cross(state.camera.front, state.camera.up));
        state.camera.up = glm::normalize(glm::cross(state.camera.right, state.camera.front));
    } else {
        state.camera.lookAt(glm::vec3(0.0f));
    }
    updateOrbitMetadata(state);

    state.sessionMessage = loadedCache
        ? std::string("Loaded session and cached planet: ") + path
        : std::string("Loaded session but cached planet is missing or outdated; click Generate Planet.");
    return true;
}

void drawSessionControls(ApplicationState& state)
{
    if (ImGui::Button("Save Local", ImVec2(0.0f, 24.0f))) {
        saveSession(state);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Local", ImVec2(0.0f, 24.0f))) {
        loadSession(state, kSessionFilePath, true);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", kSessionFilePath);

    if (!state.sessionMessage.empty()) {
        ImGui::TextWrapped("%s", state.sessionMessage.c_str());
    }
}

void drawFeatureToggle(ApplicationState& state, const char* label, bool& open)
{
    const ImVec4 activeColor(0.105f, 0.300f, 0.540f, 1.0f);
    const ImVec4 inactiveColor(0.050f, 0.090f, 0.130f, 1.0f);
    const ImVec4 hoveredColor(0.120f, 0.355f, 0.620f, 1.0f);

    ImGui::PushID(label);
    ImGui::PushStyleColor(ImGuiCol_Button, open ? activeColor : inactiveColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    if (ImGui::Button(label, ImVec2(-1.0f, 26.0f))) {
        selectFeaturePanel(state, open);
    }
    ImGui::PopStyleColor(3);
    ImGui::PopID();
}

void drawFeatureToggleRow(ApplicationState& state,
                          const char* firstLabel,
                          bool& firstOpen,
                          const char* secondLabel,
                          bool& secondOpen)
{
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float width = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
    const ImVec4 activeColor(0.105f, 0.300f, 0.540f, 1.0f);
    const ImVec4 inactiveColor(0.050f, 0.090f, 0.130f, 1.0f);
    const ImVec4 hoveredColor(0.120f, 0.355f, 0.620f, 1.0f);

    ImGui::PushID(firstLabel);
    ImGui::PushStyleColor(ImGuiCol_Button, firstOpen ? activeColor : inactiveColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    if (ImGui::Button(firstLabel, ImVec2(width, 26.0f))) {
        selectFeaturePanel(state, firstOpen);
    }
    ImGui::PopStyleColor(3);
    ImGui::PopID();

    ImGui::SameLine();
    ImGui::PushID(secondLabel);
    ImGui::PushStyleColor(ImGuiCol_Button, secondOpen ? activeColor : inactiveColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);
    if (ImGui::Button(secondLabel, ImVec2(-1.0f, 26.0f))) {
        selectFeaturePanel(state, secondOpen);
    }
    ImGui::PopStyleColor(3);
    ImGui::PopID();
}

void drawFeatureBodyBegin()
{
    ImGui::Indent(8.0f);
    ImGui::Spacing();
}

void drawFeatureBodyEnd()
{
    ImGui::Spacing();
    ImGui::Unindent(8.0f);
}

float planetDistanceScale(float planetRadius)
{
    return glm::max(planetRadius / kReferencePlanetRadius, 0.25f);
}

// 生成模块名只用于 UI 进度显示，顺序必须与 GenerationModule 枚举一致�?
const char* generationModuleLabel(int moduleIndex)
{
    static const char* kLabels[kGenerationModuleCount] = {
        "Base Terrain",
        "Initial Climate",
        "Initial Biomes",
        "Biome Terrain",
        "Erosion",
        "Final Climate",
        "Final Biomes",
        "Mesh Planning",
        "Finalize"
    };
    if (moduleIndex < 0 || moduleIndex >= kGenerationModuleCount) {
        return "";
    }
    return kLabels[moduleIndex];
}

void startPlanetGeneration(ApplicationState& state)
{
    // 如果已有后台生成任务还没完成，忽略重复点击�?
    if (state.generationFuture.valid()
        && state.generationFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }

    state.workflowStage = WorkflowStage::Generating;
    state.generationTimer = 0.0f;
    state.generationCompletedSteps.store(0, std::memory_order_relaxed);
    state.generationTotalSteps.store(1, std::memory_order_relaxed);
    state.generationActiveModule.store(0, std::memory_order_relaxed);
    for (int i = 0; i < kGenerationModuleCount; ++i) {
        state.generationModuleCompletedSteps[static_cast<std::size_t>(i)].store(0, std::memory_order_relaxed);
        state.generationModuleTotalSteps[static_cast<std::size_t>(i)].store(1, std::memory_order_relaxed);
    }
    {
        std::lock_guard<std::mutex> lock(state.generationStatusMutex);
        state.generationStatusText = "Preparing generation";
    }

    PlanetRenderSettings generatedSettings = state.renderSettings;
    copyProceduralSettings(generatedSettings, state.proceduralSettings);
    state.pendingGenerationSettings = generatedSettings;
    const int faceResolution = state.generationFaceResolution;
    const int clampedResolution = glm::clamp(faceResolution, 16, 512);
    // 主线程先估算各模块总步数，UI 可以在后台任务第一次回调前显示合理进度�?
    const int erosionIterations = glm::clamp(generatedSettings.erosionIterations, 0, 256);
    const float erosionStrength = glm::max(generatedSettings.erosionStrength, 0.0f);
    const float thermalStrength = glm::max(generatedSettings.erosionThermalStrength, 0.0f);
    const bool erosionActive = erosionIterations > 0 && (erosionStrength > 0.0f || thermalStrength > 0.0f);
    const int thermalIterations = erosionActive && thermalStrength > 0.0f
        ? glm::clamp(erosionIterations / 3, 1, 80)
        : 0;
    const int moduleTotals[kGenerationModuleCount] = {
        clampedResolution * 6,
        clampedResolution * 6 + 1,
        clampedResolution * 6 + 2,
        clampedResolution * 6 + 1,
        erosionActive ? erosionIterations + thermalIterations + 8 : 0,
        clampedResolution * 6 + 3,
        clampedResolution * 6 + 2,
        clampedResolution * 6 + 1,
        1 + 6
    };
    for (int i = 0; i < kGenerationModuleCount; ++i) {
        state.generationModuleTotalSteps[static_cast<std::size_t>(i)].store(
            std::max(moduleTotals[i], 1),
            std::memory_order_relaxed
        );
    }

    std::atomic<int>* completedSteps = &state.generationCompletedSteps;
    std::atomic<int>* totalSteps = &state.generationTotalSteps;
    std::atomic<int>* activeModule = &state.generationActiveModule;
    auto* moduleCompletedSteps = &state.generationModuleCompletedSteps;
    auto* moduleTotalSteps = &state.generationModuleTotalSteps;
    std::mutex* statusMutex = &state.generationStatusMutex;
    std::string* statusText = &state.generationStatusText;
    // 后台线程只负�?CPU 数据生成，不�?OpenGL 资源；GPU 上传留给主线程完成�?
    state.generationFuture = std::async(
        std::launch::async,
        [generatedSettings,
         faceResolution,
         completedSteps,
         totalSteps,
         activeModule,
         moduleCompletedSteps,
         moduleTotalSteps,
         statusMutex,
         statusText]() {
            auto planet = std::make_unique<PlanetProceduralData>();
            planet->generate(
                generatedSettings,
                faceResolution,
                [completedSteps, totalSteps, activeModule, moduleCompletedSteps, moduleTotalSteps, statusMutex, statusText](
                    const PlanetProceduralData::GenerationProgress& progress
                ) {
                    // 进度字段�?atomic 写入，状态文本用 mutex 保护�?
                    const int moduleIndex = glm::clamp(static_cast<int>(progress.module), 0, kGenerationModuleCount - 1);
                    completedSteps->store(progress.completedSteps, std::memory_order_relaxed);
                    totalSteps->store(std::max(progress.totalSteps, 1), std::memory_order_relaxed);
                    activeModule->store(moduleIndex, std::memory_order_relaxed);
                    (*moduleCompletedSteps)[static_cast<std::size_t>(moduleIndex)].store(
                        progress.moduleCompletedSteps,
                        std::memory_order_relaxed
                    );
                    (*moduleTotalSteps)[static_cast<std::size_t>(moduleIndex)].store(
                        std::max(progress.moduleTotalSteps, 1),
                        std::memory_order_relaxed
                    );
                    std::lock_guard<std::mutex> lock(*statusMutex);
                    *statusText = progress.status != nullptr ? progress.status : "Generating";
                }
            );
            return planet;
        }
    );
}

void finishPlanetGeneration(ApplicationState& state, std::unique_ptr<PlanetProceduralData> generatedPlanet)
{
    if (!generatedPlanet || !generatedPlanet->isGenerated()) {
        state.workflowStage = WorkflowStage::ProceduralSetup;
        state.sessionMessage = "Planet generation failed.";
        return;
    }

    const PlanetRenderSettings generatedSettings = state.pendingGenerationSettings;
    state.generatedPlanet = std::move(*generatedPlanet);
    state.renderer.settings() = generatedSettings;
    // OpenGL 纹理上传必须发生在拥�?context 的主线程�?
    state.renderer.setProceduralData(state.generatedPlanet);
    state.renderSettings = generatedSettings;
    state.hasGeneratedPlanet = true;
    state.workflowStage = WorkflowStage::Render;

    state.camera.position = glm::vec3(0.0f, generatedSettings.planetRadius * 0.45f, generatedSettings.planetRadius * 2.10f);
    state.camera.lookAt(glm::vec3(0.0f));
    updateOrbitMetadata(state);
    saveSession(state);
}

// 从渲染阶段回到生成参数界面时，把当前 renderer 设置同步�?UI�?
void returnToProceduralSetup(ApplicationState& state)
{
    if (state.hasGeneratedPlanet) {
        state.renderSettings = state.renderer.settings();
    }
    state.workflowStage = WorkflowStage::ProceduralSetup;
}

void onFramebufferSizeChanged(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}

void onMouseScrolled(GLFWwindow* window, double, double yOffset)
{
    ImGui_ImplGlfw_ScrollCallback(window, 0.0, yOffset);
    if (ImGui::GetIO().WantCaptureMouse) return;

    ApplicationState* state = getState(window);
    if (state->workflowStage != WorkflowStage::Render) {
        return;
    }

    const PlanetRenderSettings& settings = state->renderer.settings();
    // 滚轮不是�?FOV，而是沿轨道半径推�?拉远，保持星球观察视角稳定�?
    const float scrollStep = glm::max(glm::length(state->camera.position) * 0.09f, settings.planetRadius * 0.035f);
    state->camera.position += state->camera.front * (static_cast<float>(yOffset) * scrollStep);
    updateOrbitMetadata(*state);
}

void onMouseMoved(GLFWwindow* window, double xPosition, double yPosition)
{
    ImGui_ImplGlfw_CursorPosCallback(window, xPosition, yPosition);

    ApplicationState* state = getState(window);
    if (ImGui::GetIO().WantCaptureMouse) {
        state->firstLeftMouseSample = true;
        state->firstRightMouseSample = true;
        return;
    }

    if (state->workflowStage != WorkflowStage::Render) {
        state->firstLeftMouseSample = true;
        state->firstRightMouseSample = true;
        return;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        if (state->firstLeftMouseSample) {
            state->lastLeftMouseX = static_cast<float>(xPosition);
            state->lastLeftMouseY = static_cast<float>(yPosition);
            state->firstLeftMouseSample = false;
        }

        const float deltaX = static_cast<float>(xPosition) - state->lastLeftMouseX;
        const float deltaY = static_cast<float>(yPosition) - state->lastLeftMouseY;
        state->lastLeftMouseX = static_cast<float>(xPosition);
        state->lastLeftMouseY = static_cast<float>(yPosition);

        state->planetYawDegrees = wrapDegrees(state->planetYawDegrees + deltaX * 0.20f);
        state->planetPitchDegrees = wrapDegrees(state->planetPitchDegrees + deltaY * 0.20f);
        state->renderer.setPlanetRotation(state->planetYawDegrees, state->planetPitchDegrees);
    } else {
        state->firstLeftMouseSample = true;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
        if (state->firstRightMouseSample) {
            state->lastRightMouseX = static_cast<float>(xPosition);
            state->lastRightMouseY = static_cast<float>(yPosition);
            state->firstRightMouseSample = false;
        }

        const float deltaX = static_cast<float>(xPosition) - state->lastRightMouseX;
        const float deltaY = static_cast<float>(yPosition) - state->lastRightMouseY;
        state->lastRightMouseX = static_cast<float>(xPosition);
        state->lastRightMouseY = static_cast<float>(yPosition);
        state->camera.rotate(deltaX, -deltaY);
        updateOrbitMetadata(*state);
    } else {
        state->firstRightMouseSample = true;
    }
}

void onMouseButtonChanged(GLFWwindow* window, int button, int action, int modifiers)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, modifiers);

    ApplicationState* state = getState(window);
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        state->firstLeftMouseSample = true;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        state->firstRightMouseSample = true;
    }
}

void onCharacterTyped(GLFWwindow* window, unsigned int codepoint)
{
    ImGui_ImplGlfw_CharCallback(window, codepoint);
}

void onKeyPressed(GLFWwindow* window, int key, int, int action, int modifiers)
{
    ImGui_ImplGlfw_KeyCallback(window, key, 0, action, modifiers);
    if (action != GLFW_PRESS) return;

    ApplicationState* state = getState(window);
    const bool controlDown = (modifiers & GLFW_MOD_CONTROL) != 0;
    if (state->workflowStage == WorkflowStage::Render && controlDown && key == GLFW_KEY_1) {
        state->showPerformancePanel = !state->showPerformancePanel;
        return;
    }

    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (state->workflowStage != WorkflowStage::Render) {
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
        if (key == GLFW_KEY_TAB) {
            state->showDebugPanel = !state->showDebugPanel;
        }
        return;
    }

    PlanetRenderSettings& settings = state->renderer.settings();

    // 快捷键主要循�?debug/render mode，具体连续参数仍�?ImGui 控件调整�?
    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
    if (key == GLFW_KEY_1) {
        const int nextMode = (static_cast<int>(settings.renderMode) + 1) % 4;
        settings.renderMode = static_cast<PlanetRenderMode>(nextMode);
    }
    if (key == GLFW_KEY_2) {
        const int nextWireMode = (static_cast<int>(settings.wireMode) + 1) % 4;
        settings.wireMode = static_cast<PlanetWireMode>(nextWireMode);
    }
    if (key == GLFW_KEY_3) {
        settings.wireMode = settings.wireMode == PlanetWireMode::MountainMask
            ? PlanetWireMode::None
            : PlanetWireMode::MountainMask;
    }
    if (key == GLFW_KEY_4) {
        if (settings.renderTerrain && settings.renderOcean) {
            settings.renderOcean = false;
        } else if (settings.renderTerrain && !settings.renderOcean) {
            settings.renderTerrain = false;
            settings.renderOcean = true;
        } else {
            settings.renderTerrain = true;
            settings.renderOcean = true;
        }
    }
    if (key == GLFW_KEY_TAB) {
        state->showDebugPanel = !state->showDebugPanel;
    }
}

void handleKeyboardMovement(GLFWwindow* window, ApplicationState& state)
{
    if (state.workflowStage != WorkflowStage::Render) return;
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        state.camera.move(FlyCamera::MovementDirection::Forward, state.deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        state.camera.move(FlyCamera::MovementDirection::Backward, state.deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        state.camera.move(FlyCamera::MovementDirection::Left, state.deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        state.camera.move(FlyCamera::MovementDirection::Right, state.deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        state.camera.move(FlyCamera::MovementDirection::Down, state.deltaSeconds);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        state.camera.move(FlyCamera::MovementDirection::Up, state.deltaSeconds);
    }

    updateOrbitMetadata(state);
}

void printControls()
{
    std::cout << "\n=== Procedural Planet Controls ===\n";
    std::cout << "  W/S       : move forward/backward\n";
    std::cout << "  A/D       : move left/right\n";
    std::cout << "  Q/E       : move down/up\n";
    std::cout << "  LMB+drag  : rotate planet\n";
    std::cout << "  RMB+drag  : rotate camera\n";
    std::cout << "  Scroll    : dolly camera toward/away from view direction\n";
    std::cout << "  1         : cycle render mode\n";
    std::cout << "  2         : cycle wire overlay\n";
    std::cout << "  4         : cycle land/ocean visibility\n";
    std::cout << "  Ctrl+1    : toggle performance monitor\n";
    std::cout << "  Tab       : toggle ImGui panel\n";
    std::cout << "  ESC       : quit\n\n";
}

void drawProceduralPanel(ApplicationState& state)
{
    PlanetRenderSettings& settings = state.proceduralSettings;

    ImGui::Text("Procedural Generation");
    ImGui::Separator();
    drawSessionControls(state);
    ImGui::Separator();

    if (state.workflowStage == WorkflowStage::Generating) {
        // 生成期间只显示进度，不允许同时修改输入参数�?
        const int completedSteps = state.generationCompletedSteps.load(std::memory_order_relaxed);
        const int totalSteps = std::max(state.generationTotalSteps.load(std::memory_order_relaxed), 1);
        const float progress = glm::clamp(
            static_cast<float>(completedSteps) / static_cast<float>(totalSteps),
            0.0f,
            1.0f
        );
        std::string generationStatus;
        {
            std::lock_guard<std::mutex> lock(state.generationStatusMutex);
            generationStatus = state.generationStatusText;
        }
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), "Generating planet");
        const int activeModuleIndex = glm::clamp(
            state.generationActiveModule.load(std::memory_order_relaxed),
            0,
            kGenerationModuleCount - 1
        );
        const int activeModuleCompleted = state.generationModuleCompletedSteps[static_cast<std::size_t>(activeModuleIndex)].load(std::memory_order_relaxed);
        const int activeModuleTotal = std::max(
            state.generationModuleTotalSteps[static_cast<std::size_t>(activeModuleIndex)].load(std::memory_order_relaxed),
            1
        );
        ImGui::Text(
            "Generating... (%d/%d)",
            activeModuleCompleted,
            activeModuleTotal
        );
        ImGui::TextDisabled(
            "%s / %s",
            generationModuleLabel(activeModuleIndex),
            generationStatus.c_str()
        );
        return;
    }

    const float proceduralDistanceScale = planetDistanceScale(settings.planetRadius);
    ImGui::Text("Feature Panels");
    drawFeatureToggleRow(state, "Terrain Bake", state.showProceduralTerrainFeature, "Erosion Bake", state.showProceduralErosionFeature);
    if (ImGui::Button("Preset: Rugged Rivers", ImVec2(-1.0f, 28.0f))) {
        settings.terrainHeightScale = 24.0f * proceduralDistanceScale;
        settings.terrainNoiseScale = 0.58f;
        settings.mountainMaskStrength = 0.55f;
        settings.mountainMaskScale = 1.95f;
        settings.mountainRidgeSharpness = 2.85f;
        settings.erosionIterations = 0;
        settings.erosionStrength = 0.0f;
        settings.erosionSediment = 0.72f;
        settings.erosionThermalStrength = 0.0f;
        settings.terrainMaterialNoiseStrength = 0.0f;
        settings.riverVisibility = 1.70f;
        settings.riverWidth = 0.72f;
        settings.riverShine = 1.05f;
        settings.riverRefractionStrength = 0.55f;
    }
    ImGui::Separator();
    // 下面是会影响 CPU 烘焙的参数：地形高度、山脉、侵蚀、材质权重等�?
    if (state.showProceduralTerrainFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Terrain Bake");
        ImGui::SliderFloat("Planet Radius", &settings.planetRadius, 50.0f, 5000.0f, "%.1f");
        ImGui::SliderFloat("Sea Level", &settings.seaLevelOffset, -1.5f, 1.5f, "%.2f");
        ImGui::SliderFloat("Height Scale", &settings.terrainHeightScale, 0.0f, 80.0f * proceduralDistanceScale, "%.2f");
        ImGui::SliderFloat("Noise Scale", &settings.terrainNoiseScale, 0.2f, 10.0f, "%.2f");
        ImGui::SliderFloat("Mountain Mask", &settings.mountainMaskStrength, 0.0f, 2.4f, "%.2f");
        ImGui::SliderFloat("Mountain Scale", &settings.mountainMaskScale, 0.5f, 8.0f, "%.2f");
        ImGui::SliderFloat("Ridge Sharpness", &settings.mountainRidgeSharpness, 1.0f, 6.0f, "%.2f");
        ImGui::SliderInt("Face Resolution", &state.generationFaceResolution, 256, 512);
        drawFeatureBodyEnd();
    }
    if (state.showProceduralErosionFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Erosion Bake");
        ImGui::SliderInt("Iterations", &settings.erosionIterations, 0, 160);
        ImGui::SliderFloat("Strength", &settings.erosionStrength, 0.0f, 0.14f, "%.3f");
        ImGui::SliderFloat("Talus", &settings.erosionTalus, 0.005f, 0.12f, "%.3f");
        ImGui::SliderFloat("Sediment", &settings.erosionSediment, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Thermal", &settings.erosionThermalStrength, 0.0f, 0.08f, "%.3f");
        drawFeatureBodyEnd();
    }

    if (state.showProceduralMaterialFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Terrain Materials");
        ImGui::ColorEdit3("Lowland Color", &settings.terrainLowlandColor.x);
        ImGui::ColorEdit3("Forest Color", &settings.terrainForestColor.x);
        ImGui::ColorEdit3("Desert Color", &settings.terrainDesertColor.x);
        ImGui::ColorEdit3("Rock Color", &settings.terrainRockColor.x);
        ImGui::ColorEdit3("Beach Color", &settings.terrainBeachColor.x);
        ImGui::ColorEdit3("Snow Color", &settings.terrainSnowColor.x);
        ImGui::SliderFloat("Beach Width", &settings.terrainBeachWidth, 0.005f, 0.20f, "%.3f");
        ImGui::SliderFloat("Rock Slope Start", &settings.terrainRockSlopeStart, 0.0f, 0.8f, "%.2f");
        ImGui::SliderFloat("Rock Slope End", &settings.terrainRockSlopeEnd, settings.terrainRockSlopeStart + 0.01f, 1.0f, "%.2f");
        ImGui::SliderFloat("Snow Start", &settings.terrainSnowStart, 0.2f, 1.0f, "%.2f");
        ImGui::SliderFloat("Snow End", &settings.terrainSnowEnd, settings.terrainSnowStart + 0.01f, 1.2f, "%.2f");
        ImGui::SliderFloat("Material Noise", &settings.terrainMaterialNoiseStrength, 0.0f, 0.4f, "%.2f");
        drawFeatureBodyEnd();
    }

    ImGui::Spacing();
    if (state.generatedPlanet.isGenerated()) {
        ImGui::Text("Last bake: %d x %d x 6", state.generatedPlanet.resolution(), state.generatedPlanet.resolution());
        ImGui::Text("Height: %.3f to %.3f", state.generatedPlanet.minHeight(), state.generatedPlanet.maxHeight());
        ImGui::Text("Water: %.1f%% | Shore: %.1f%%",
                    state.generatedPlanet.waterCoverage() * 100.0f,
                    state.generatedPlanet.shoreCoverage() * 100.0f);
        ImGui::Separator();
    }

    if (ImGui::Button("Generate Planet", ImVec2(-1.0f, 32.0f))) {
        startPlanetGeneration(state);
    }
}

void drawRenderModeControls(PlanetRenderSettings& settings)
{
    ImGui::Text("Render Mode");
    int renderModeIndex = 1;
    if (settings.renderMode == PlanetRenderMode::Unshaded) renderModeIndex = 0;
    if (settings.renderMode == PlanetRenderMode::Shaded) renderModeIndex = 1;
    if (settings.renderMode == PlanetRenderMode::HeightMap) renderModeIndex = 2;
    if (settings.renderMode == PlanetRenderMode::Normals) renderModeIndex = 3;
    if (ImGui::RadioButton("Unshaded", renderModeIndex == 0)) settings.renderMode = PlanetRenderMode::Unshaded;
    ImGui::SameLine();
    if (ImGui::RadioButton("Shaded", renderModeIndex == 1)) settings.renderMode = PlanetRenderMode::Shaded;
    ImGui::SameLine();
    if (ImGui::RadioButton("Height", renderModeIndex == 2)) settings.renderMode = PlanetRenderMode::HeightMap;
    ImGui::SameLine();
    if (ImGui::RadioButton("Normals", renderModeIndex == 3)) settings.renderMode = PlanetRenderMode::Normals;

    int wireModeIndex = static_cast<int>(settings.wireMode);
    if (ImGui::RadioButton("No Wire", wireModeIndex == 0)) settings.wireMode = PlanetWireMode::None;
    ImGui::SameLine();
    if (ImGui::RadioButton("Water Mesh", wireModeIndex == 1)) settings.wireMode = PlanetWireMode::Ocean;
    ImGui::SameLine();
    if (ImGui::RadioButton("Baked LOD", wireModeIndex == 2)) settings.wireMode = PlanetWireMode::BakedLod;
    ImGui::SameLine();

}

void drawRenderPanel(ApplicationState& state)
{
    PlanetRenderSettings& settings = state.renderer.settings();
    const float renderDistanceScale = planetDistanceScale(settings.planetRadius);

    if (ImGui::Button("Back To Procedural", ImVec2(-1.0f, 28.0f))) {
        returnToProceduralSetup(state);
        return;
    }

    drawSessionControls(state);
    ImGui::Separator();
    if (state.generatedPlanet.isGenerated()) {
        ImGui::Text("Generated data: %d face resolution", state.generatedPlanet.resolution());
        ImGui::Text("Offline terrain chunks: %zu", state.generatedPlanet.terrainChunks().size());
        ImGui::Text("Feature segments: river %zu | coast %zu | ridge %zu | erosion %zu",
                    state.generatedPlanet.terrainFeatureSegmentCount(PlanetProceduralData::TerrainFeatureType::River),
                    state.generatedPlanet.terrainFeatureSegmentCount(PlanetProceduralData::TerrainFeatureType::Coast),
                    state.generatedPlanet.terrainFeatureSegmentCount(PlanetProceduralData::TerrainFeatureType::Ridge),
                    state.generatedPlanet.terrainFeatureSegmentCount(PlanetProceduralData::TerrainFeatureType::ErosionEdge));
        ImGui::Text("Water %.1f%% | Shore %.1f%% | Max depth %.2f",
                    state.generatedPlanet.waterCoverage() * 100.0f,
                    state.generatedPlanet.shoreCoverage() * 100.0f,
                    state.generatedPlanet.maxWaterDepth());
    }

    ImGui::Separator();
    ImGui::Text("Render Features");
    drawFeatureToggleRow(state, "Visibility", state.showVisibilityFeature, "Terrain Runtime", state.showTerrainRuntimeFeature);
    drawFeatureToggleRow(state, "Rivers", state.showRiverFeature, "Atmosphere", state.showAtmosphereFeature);
    drawFeatureToggleRow(state, "Ocean Color", state.showOceanColorFeature, "Ocean Waves", state.showOceanWaveFeature);
    drawFeatureToggleRow(state, "Ocean Material", state.showOceanMaterialFeature, "Planar Targets", state.showOceanReflectionFeature);
    if (!settings.renderOcean) {
        ImGui::TextDisabled("Ocean-dependent effects are hidden while Ocean is off.");
    }

    ImGui::Separator();
    ImGui::Text("Utility Panels");
    drawFeatureToggle(state, "Render Debug", state.showRenderModeFeature);
    drawFeatureToggleRow(state, "Lighting", state.showLightingFeature, "Advanced Render", state.showAdvancedRenderFeature);
    drawFeatureToggle(state, "Camera", state.showCameraFeature);
    ImGui::Separator();

    if (state.showRenderModeFeature) {
        drawFeatureBodyBegin();
        drawRenderModeControls(settings);
        drawFeatureBodyEnd();
    }

    if (state.showVisibilityFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Visibility");
        ImGui::Checkbox("Terrain", &settings.renderTerrain);
        ImGui::Checkbox("Ocean", &settings.renderOcean);
        drawFeatureBodyEnd();
    }

    if (state.showTerrainRuntimeFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Terrain Runtime");
        ImGui::Checkbox("Terrain Enabled", &settings.renderTerrain);
        ImGui::TextDisabled("Immediate shader controls; bake-only terrain, biome, erosion need Generate Planet.");
        ImGui::SliderFloat("Height Scale", &settings.terrainHeightScale, 0.0f, 120.0f * renderDistanceScale, "%.2f");
        ImGui::SliderFloat("Sea Level", &settings.seaLevelOffset, -1.5f, 1.5f, "%.2f");
        ImGui::SliderFloat("Beach Width", &settings.terrainBeachWidth, 0.005f, 0.25f, "%.3f");
        ImGui::SliderFloat("Rock Slope Start", &settings.terrainRockSlopeStart, 0.0f, 0.8f, "%.2f");
        ImGui::SliderFloat("Rock Slope End", &settings.terrainRockSlopeEnd, settings.terrainRockSlopeStart + 0.01f, 1.0f, "%.2f");
        ImGui::SliderFloat("Snow Start", &settings.terrainSnowStart, 0.2f, 1.0f, "%.2f");
        ImGui::SliderFloat("Snow End", &settings.terrainSnowEnd, settings.terrainSnowStart + 0.01f, 1.2f, "%.2f");
        ImGui::SliderFloat("Material Noise", &settings.terrainMaterialNoiseStrength, 0.0f, 0.4f, "%.2f");
        drawFeatureBodyEnd();
    }

    if (state.showLightingFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Lighting");
        ImGui::ColorEdit3("Sky Color", &settings.skyColor.x);
        ImGui::SliderFloat("Fog Density", &settings.fogDensity, 0.0f, 0.00003f, "%.6f");
        drawFeatureBodyEnd();
    }

    if (state.showAtmosphereFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Atmosphere");
        ImGui::Checkbox("Atmosphere Enabled", &settings.renderAtmosphere);
        ImGui::SliderFloat("Atmosphere Height", &settings.atmosphereHeight, 1.0f, 120.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Atmosphere Density", &settings.atmosphereDensity, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Rayleigh", &settings.atmosphereRayleighStrength, 0.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("Mie", &settings.atmosphereMieStrength, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Mie G", &settings.atmosphereMieAnisotropy, 0.0f, 0.95f, "%.2f");
        ImGui::SliderFloat("Atmosphere Exposure", &settings.atmosphereExposure, 0.1f, 4.0f, "%.2f");
        ImGui::ColorEdit3("Rayleigh Color", &settings.atmosphereRayleighColor.x);
        ImGui::ColorEdit3("Mie Color", &settings.atmosphereMieColor.x);
        ImGui::Separator();
        ImGui::Checkbox("Clouds Enabled", &settings.renderClouds);
        ImGui::SliderFloat("Cloud Coverage", &settings.cloudCoverage, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Cloud Sharpness", &settings.cloudSharpness, 0.4f, 3.0f, "%.2f");
        ImGui::SliderFloat("Cloud Scale", &settings.cloudScale, 1.0f, 12.0f, "%.2f");
        ImGui::SliderFloat("Cloud Speed", &settings.cloudSpeed, -0.08f, 0.08f, "%.3f");
        ImGui::SliderFloat("Cloud Spin", &settings.cloudRotationSpeed, -20.0f, 20.0f, "%.2f");
        ImGui::SliderFloat("Cloud Height", &settings.cloudHeight, 1.0f, glm::max(settings.atmosphereHeight - 1.0f, 1.0f), "%.1f");
        ImGui::SliderFloat("Cloud Opacity", &settings.cloudOpacity, 0.0f, 1.0f, "%.2f");
        ImGui::ColorEdit3("Cloud Color", &settings.cloudColor.x);
        drawFeatureBodyEnd();
    }

    if (state.showRiverFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Rivers");
        ImGui::Checkbox("Terrain Enabled", &settings.renderTerrain);
        ImGui::Checkbox("Rivers Enabled", &settings.renderRivers);
        ImGui::ColorEdit3("River Color", &settings.riverColor.x);
        ImGui::SliderFloat("Visibility", &settings.riverVisibility, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Width", &settings.riverWidth, 0.08f, 1.20f, "%.2f");
        ImGui::SliderFloat("Shine", &settings.riverShine, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Refraction", &settings.riverRefractionStrength, 0.0f, 1.0f, "%.2f");
        drawFeatureBodyEnd();
    }

    if (state.showOceanColorFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Ocean Color");
        ImGui::Checkbox("Ocean Enabled", &settings.renderOcean);
        ImGui::SliderFloat("Opacity Limit", &settings.oceanAlpha, 0.05f, 1.0f, "%.2f");
        ImGui::ColorEdit3("Shallow Color", &settings.oceanShallowColor.x);
        ImGui::ColorEdit3("Deep Color", &settings.oceanDeepColor.x);
        ImGui::ColorEdit3("SSS Color", &settings.oceanSSSColor.x);
        drawFeatureBodyEnd();
    }

    if (state.showOceanWaveFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Ocean Waves");
        ImGui::Checkbox("Ocean Enabled", &settings.renderOcean);
        ImGui::Checkbox("Ocean Waves Enabled", &settings.renderOceanWaves);
        ImGui::SliderFloat("Wave Height", &settings.oceanWaveAmplitude, 0.0f, 0.80f, "%.3f");
        ImGui::SliderFloat("Choppiness", &settings.oceanChoppiness, 0.0f, 0.80f, "%.3f");
        ImGui::SliderFloat("Wave Tile Scale", &settings.oceanWaveTileScale, 4.0f, 28.0f, "%.1f");
        ImGui::SliderFloat("FFT Normal", &settings.oceanWaveNormalStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderInt("FFT Cascades", &settings.oceanFftCascadeCount, 1, 3);
        ImGui::SliderInt("FFT Frame Stride", &settings.oceanFftFrameStride, 1, 8);
        drawFeatureBodyEnd();
    }

    if (state.showOceanMaterialFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Ocean Material");
        ImGui::Checkbox("Ocean Enabled", &settings.renderOcean);
        ImGui::Checkbox("Ocean Material Enabled", &settings.renderOceanMaterial);
        ImGui::SliderFloat("Fresnel", &settings.oceanFresnelStrength, 0.1f, 3.0f, "%.2f");
        ImGui::SliderFloat("Refraction Distortion", &settings.oceanDistortionStrength, 0.0f, 0.08f, "%.3f");
        ImGui::SliderFloat("Depth Blend", &settings.oceanDepthRange, 0.5f, 40.0f, "%.2f");
        ImGui::SliderFloat("Shallow Depth", &settings.oceanShallowDepthRange, 0.02f, 4.0f, "%.2f");
        ImGui::SliderFloat("Depth Scale", &settings.oceanDepthScale, 0.5f, 20.0f, "%.2f");
        ImGui::SliderFloat("Shallow Opacity", &settings.oceanShallowAlpha, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Deep Opacity", &settings.oceanDeepAlpha, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Tint Strength", &settings.oceanTintStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Detail Normal", &settings.oceanDetailNormalStrength, 0.0f, 0.8f, "%.2f");
        ImGui::SliderFloat("Detail Scale", &settings.oceanDetailNormalScale, 4.0f, 96.0f, "%.1f");
        ImGui::SliderFloat("Detail Fade", &settings.oceanDetailFadeDistance, 40.0f, 1200.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Specular Strength", &settings.oceanSpecularStrength, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Specular Sharpness", &settings.oceanSpecularSharpness, 0.5f, 4.0f, "%.2f");
        ImGui::SliderFloat("Water Roughness", &settings.oceanRoughness, 0.02f, 0.35f, "%.3f");
        ImGui::SliderFloat("SSS Strength", &settings.oceanSSSStrength, 0.0f, 0.8f, "%.2f");
        ImGui::SliderFloat("SSS Power", &settings.oceanSSSPower, 1.0f, 8.0f, "%.1f");
        ImGui::SliderFloat("Shore Blend", &settings.oceanShoreBlendWidth, 0.01f, 0.5f, "%.3f");
        drawFeatureBodyEnd();
    }

    if (state.showOceanReflectionFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Ocean Reflection");
        ImGui::Checkbox("Ocean Enabled", &settings.renderOcean);
        ImGui::Checkbox("Planar Targets Enabled", &settings.renderOceanReflectionRefraction);
        // 反射/折射 target 支持降采样和隔帧更新，减少水面效果的额外 draw cost�?
        ImGui::Checkbox("Reflection", &settings.renderOceanReflection);
        ImGui::Checkbox("Refraction", &settings.renderOceanRefraction);
        ImGui::SliderFloat("Target Scale", &settings.oceanReflectionResolutionScale, 0.25f, 1.0f, "%.2f");
        ImGui::SliderInt("Reflection Stride", &settings.oceanReflectionFrameStride, 1, 8);
        ImGui::SliderInt("Refraction Stride", &settings.oceanRefractionFrameStride, 1, 8);
        ImGui::Checkbox("Auto Distance LOD", &settings.oceanAutoDistanceLod);
        ImGui::SliderFloat("Reflection Max Alt", &settings.oceanReflectionMaxAltitude, 40.0f, 2000.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Refraction Max Alt", &settings.oceanRefractionMaxAltitude, 10.0f, 800.0f * renderDistanceScale, "%.1f");
        drawFeatureBodyEnd();
    }

    if (state.showAdvancedRenderFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Advanced Render");
        ImGui::TextDisabled("Distance scale: %.2fx from radius %.0f", renderDistanceScale, kReferencePlanetRadius);
        ImGui::SliderFloat("Ocean Tess Max", &settings.oceanTessellationMax, 1.0f, 4.0f, "%.1f");
        ImGui::SliderFloat("Ocean Tess Min", &settings.oceanTessellationMin, 1.0f, settings.oceanTessellationMax, "%.1f");
        ImGui::SliderFloat("Ocean Tess Near", &settings.oceanTessellationNearDistance, 10.0f, 400.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Ocean Tess Far", &settings.oceanTessellationFarDistance, settings.oceanTessellationNearDistance + 10.0f, 1400.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Near Plane", &settings.cameraNearPlane, 0.20f, 5.0f, "%.2f");
        ImGui::SliderFloat("Far Plane", &settings.cameraFarPlane, 1000.0f, 12000.0f * renderDistanceScale, "%.0f");
        drawFeatureBodyEnd();
    }

    if (state.showCameraFeature) {
        drawFeatureBodyBegin();
        ImGui::Text("Camera");
        ImGui::Text("Distance: %.1f", glm::length(state.camera.position));
        ImGui::Text("Yaw %.1f | Pitch %.1f", state.cameraOrbitYawDegrees, state.cameraOrbitPitchDegrees);
        ImGui::SliderFloat("Mouse Sensitivity", &state.camera.mouseSensitivity, 0.02f, 0.5f, "%.2f");
        ImGui::Text("Position: %.1f %.1f %.1f", state.camera.position.x, state.camera.position.y, state.camera.position.z);
        ImGui::Text("Front: %.2f %.2f %.2f", state.camera.front.x, state.camera.front.y, state.camera.front.z);
        ImGui::Text("Altitude: %.1f", glm::max(glm::length(state.camera.position) - settings.planetRadius, 0.0f));
        ImGui::Text("FOV: %.1f", state.camera.fieldOfView);
        drawFeatureBodyEnd();
    }

    state.renderSettings = settings;
}

void drawPerformancePanel(ApplicationState& state)
{
    if (!state.showPerformancePanel || state.workflowStage != WorkflowStage::Render) return;

    const ImVec4 black(0.015f, 0.013f, 0.010f, 0.96f);
    const ImVec4 panel(0.055f, 0.048f, 0.032f, 0.98f);
    const ImVec4 gold(0.96f, 0.70f, 0.23f, 1.0f);
    const ImVec4 mutedGold(0.70f, 0.52f, 0.22f, 1.0f);
    const ImVec4 text(0.98f, 0.90f, 0.72f, 1.0f);

    ImGui::SetNextWindowPos(ImVec2(420.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(340.0f, 0.0f), ImVec2(440.0f, 680.0f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, black);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, panel);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.20f, 0.14f, 0.04f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, mutedGold);
    ImGui::PushStyleColor(ImGuiCol_Text, text);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.24f, 0.16f, 0.04f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.35f, 0.24f, 0.07f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.45f, 0.30f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Separator, mutedGold);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.4f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);

    if (ImGui::Begin("Performance Monitor  Ctrl+1", &state.showPerformancePanel, ImGuiWindowFlags_AlwaysAutoResize)) {
        const float fps = 1.0f / glm::max(state.deltaSeconds, 0.0001f);
        const PlanetRenderer::PerformanceStats& perf = state.renderer.performanceStats();
        const PlanetRenderer::CullingStats& cullingStats = state.renderer.cullingStats();

        // 这些时间�?CPU 提交侧统计，不包�?glFinish �?GPU 阻塞等待�?
        ImGui::PushStyleColor(ImGuiCol_Text, gold);
        ImGui::Text("CPU Submit Timings");
        ImGui::PopStyleColor();
        ImGui::Text("Not blocking GPU");
        ImGui::Separator();

        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Camera distance: %.1f", glm::length(state.camera.position));
        ImGui::Text("Camera altitude: %.1f", glm::max(glm::length(state.camera.position) - state.renderer.settings().planetRadius, 0.0f));
        ImGui::Text("Total: %.2f ms", perf.totalMs);
        ImGui::Text("Culling: %.2f ms", perf.cullingMs);
        ImGui::Text("FFT: %.2f ms %s", perf.fftMs, perf.fftUpdated ? "updated" : "reused");
        ImGui::Text("Reflection/Refraction: %.2f ms", perf.reflectionRefractionMs);
        ImGui::Text("Terrain: %.2f ms | Ocean: %.2f ms", perf.terrainMs, perf.oceanMs);
        ImGui::Text("Atmosphere: %.2f ms | Wire: %.2f ms", perf.atmosphereMs, perf.wireMs);

        ImGui::Separator();
        ImGui::Text("Terrain path: Baked chunks");
        ImGui::Text("Baked chunks: %zu / %zu", perf.visibleBakedChunkCount, perf.bakedChunkCount);
        ImGui::Text("Baked LOD: full %zu | mid %zu | low %zu",
                    perf.visibleBakedChunkLodCount[0],
                    perf.visibleBakedChunkLodCount[1],
                    perf.visibleBakedChunkLodCount[2]);
        ImGui::Text("Ocean patches: %zu", perf.oceanPatchCount);
        ImGui::Text("LOD nodes: %zu | Split: %zu", cullingStats.visitedNodes, cullingStats.splitNodes);
        ImGui::Text("Culled: %zu frustum | %zu horizon", cullingStats.frustumCulledNodes, cullingStats.horizonCulledNodes);
        ImGui::Text("Ocean tess: %.2f", perf.effectiveOceanTessMax);
        ImGui::Text("Est triangles: land %zu | water %zu", perf.estimatedTerrainTriangles, perf.estimatedOceanTriangles);

        ImGui::Separator();
        ImGui::Text("FFT cascades: %d | stride: %d", perf.fftCascadeCount, perf.fftFrameStride);
        ImGui::Text("Reflection: %s | Refraction: %s",
                    perf.reflectionEnabled ? (perf.reflectionUpdated ? "updated" : "reused") : "off",
                    perf.refractionEnabled ? (perf.refractionUpdated ? "updated" : "reused") : "off");
        ImGui::Text("Refl weight: %.2f | Refr weight: %.2f", perf.reflectionWeight, perf.refractionWeight);
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(9);
}

void drawDebugPanel(ApplicationState& state)
{
    if (!state.showDebugPanel) return;

    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(360.0f, 0.0f), ImVec2(460.0f, 700.0f));

    const char* title = state.workflowStage == WorkflowStage::Render
        ? "Render Controls"
        : "Procedural Generation";

    // 调试面板使用单独配色，不影响 ImGui 全局主题�?
    const ImVec4 deepNavy(0.018f, 0.035f, 0.070f, 0.96f);
    const ImVec4 panelBlue(0.045f, 0.105f, 0.180f, 0.98f);
    const ImVec4 activeBlue(0.070f, 0.210f, 0.360f, 1.0f);
    const ImVec4 lineBlue(0.115f, 0.340f, 0.580f, 1.0f);
    const ImVec4 brightBlue(0.180f, 0.560f, 0.920f, 1.0f);
    const ImVec4 hoverBlue(0.130f, 0.420f, 0.720f, 0.92f);
    const ImVec4 textBlue(0.820f, 0.930f, 1.000f, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, deepNavy);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, panelBlue);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, activeBlue);
    ImGui::PushStyleColor(ImGuiCol_Border, lineBlue);
    ImGui::PushStyleColor(ImGuiCol_Text, textBlue);
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.060f, 0.170f, 0.290f, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hoverBlue);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, activeBlue);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.035f, 0.080f, 0.135f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.070f, 0.180f, 0.310f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.090f, 0.245f, 0.430f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, brightBlue);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.380f, 0.760f, 1.000f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, brightBlue);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.070f, 0.185f, 0.320f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.105f, 0.300f, 0.540f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.140f, 0.420f, 0.760f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Separator, lineBlue);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.4f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

    if (!ImGui::Begin(title, &state.showDebugPanel, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(18);
        return;
    }

    if (state.workflowStage == WorkflowStage::Render) {
        drawRenderPanel(state);
    } else {
        drawProceduralPanel(state);
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(18);
}

void configureImGuiFonts()
{
    ImGuiIO& io = ImGui::GetIO();
    const char* fontPaths[] = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyh.ttf",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/NotoSansCJK-Regular.ttc"
    };

    for (const char* fontPath : fontPaths) {
        if (!std::filesystem::exists(fontPath)) {
            continue;
        }

        ImFontConfig config;
        config.OversampleH = 2;
        config.OversampleV = 2;
        config.PixelSnapH = true;
        if (io.Fonts->AddFontFromFileTTF(fontPath, 16.0f, &config, io.Fonts->GetGlyphRangesChineseFull()) != nullptr) {
            return;
        }
    }

    // 找不到中文字体时退回默认字体；UI 文本仍以英文为主�?
    io.Fonts->AddFontDefault();
}
} // namespace

int main(int argc, char** argv)
{
    bool profileEnabled = false;
    int profileFrameLimit = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i] != nullptr ? argv[i] : "";
        if (argument == "--profile") {
            profileEnabled = true;
        } else if (argument == "--profile-frames" && i + 1 < argc) {
            profileFrameLimit = std::max(std::atoi(argv[++i]), 0);
        }
    }
    if (const char* envProfile = std::getenv("PROW_PROFILE")) {
        profileEnabled = std::string(envProfile) == "1";
    }
    if (profileEnabled) {
        Engine::Instrumentor::GetInstance().BeginSession(kProfileTraceFilePath);
        std::cout << "[Profiler] Writing trace to " << kProfileTraceFilePath << "\n";
        if (profileFrameLimit > 0) {
            std::cout << "[Profiler] Auto-exit after " << profileFrameLimit << " frames\n";
        }
    }

    // 1. 创建窗口�?OpenGL context�?
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        if (profileEnabled) {
            Engine::Instrumentor::GetInstance().EndSession();
        }
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Procedural Planet | Tessellation", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        if (profileEnabled) {
            Engine::Instrumentor::GetInstance().EndSession();
        }
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // 2. 注册输入回调。ApplicationState 挂到 GLFW user pointer 供回调访问�?
    ApplicationState appState;
    glfwSetWindowUserPointer(window, &appState);
    glfwSetFramebufferSizeCallback(window, onFramebufferSizeChanged);
    glfwSetScrollCallback(window, onMouseScrolled);
    glfwSetCursorPosCallback(window, onMouseMoved);
    glfwSetMouseButtonCallback(window, onMouseButtonChanged);
    glfwSetKeyCallback(window, onKeyPressed);
    glfwSetCharCallback(window, onCharacterTyped);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwTerminate();
        if (profileEnabled) {
            Engine::Instrumentor::GetInstance().EndSession();
        }
        return -1;
    }

    GLint maxPatchVertices = 0;
    glGetIntegerv(GL_MAX_PATCH_VERTICES, &maxPatchVertices);
    std::cout << "GL_MAX_PATCH_VERTICES = " << maxPatchVertices << "\n";
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "OpenGL vendor: " << glGetString(GL_VENDOR) << "\n";
    std::cout << "OpenGL renderer: " << glGetString(GL_RENDERER) << "\n";

    // 3. 初始�?ImGui 和渲染器资源�?
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    configureImGuiFonts();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init("#version 410");

    appState.renderer.initialize();
    appState.renderSettings = appState.renderer.settings();
    appState.proceduralSettings = appState.renderer.settings();
    // 4. 尝试恢复本地 session/cache；成功时可直接进入渲染阶段�?
    loadSession(appState, kSessionFilePath, false);
    appState.renderer.setPlanetRotation(appState.planetYawDegrees, appState.planetPitchDegrees);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    printControls();

    int profiledFrameCount = 0;
    while (!glfwWindowShouldClose(window))
    {
        PROFILE_SCOPE("MainLoop Frame");
        // 帧时间用于相机、自动旋转、反射权重平滑和 FFT 更新节流�?
        const float currentTime = static_cast<float>(glfwGetTime());
        appState.deltaSeconds = currentTime - appState.previousFrameTime;
        appState.previousFrameTime = currentTime;

        if (appState.workflowStage == WorkflowStage::Generating) {
            // 后台生成完成后，在主线程取出结果并上�?GPU 资源�?
            appState.generationTimer += appState.deltaSeconds;
            if (appState.generationFuture.valid()
                && appState.generationFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try {
                    PROFILE_SCOPE("Finish Planet Generation");
                    finishPlanetGeneration(appState, appState.generationFuture.get());
                } catch (const std::exception& exception) {
                    appState.workflowStage = WorkflowStage::ProceduralSetup;
                    appState.sessionMessage = std::string("Planet generation failed: ") + exception.what();
                }
            }
        }

        {
            PROFILE_SCOPE("Input Update");
            handleKeyboardMovement(window, appState);
        }
        if (appState.workflowStage == WorkflowStage::Render) {
            // Render 阶段保持星球静止，方便观察河流、LOD 和材质 debug；需要旋转时用右键拖拽。
            appState.renderer.setPlanetRotation(appState.planetYawDegrees, appState.planetPitchDegrees);
            updateOrbitCamera(appState, appState.renderer.settings());
        }

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        framebufferWidth = glm::max(framebufferWidth, 1);
        framebufferHeight = glm::max(framebufferHeight, 1);

        const PlanetRenderSettings& activeSettings = appState.workflowStage == WorkflowStage::Render
            ? appState.renderer.settings()
            : appState.renderSettings;

        // 投影矩阵使用当前设置里的 near/far，支持大半径星球的远裁剪面�?
        const glm::mat4 projectionMatrix = glm::perspective(
            glm::radians(appState.camera.fieldOfView),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            activeSettings.cameraNearPlane,
            activeSettings.cameraFarPlane
        );
        const glm::mat4 viewMatrix = appState.camera.viewMatrix();

        const glm::vec3 skyColor = appState.workflowStage == WorkflowStage::Render
            ? appState.renderer.settings().skyColor
            : glm::vec3(0.018f, 0.035f, 0.070f);
        {
            PROFILE_SCOPE("GL Clear");
            glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        {
            PROFILE_SCOPE("ImGui NewFrame");
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
        }

        // UI 先生�?draw data，星球渲染后再提�?ImGui，保证面板覆盖在画面上方�?
        {
            PROFILE_SCOPE("ImGui Build Panels");
            drawDebugPanel(appState);
            drawPerformancePanel(appState);
        }
        if (appState.workflowStage == WorkflowStage::Render) {
            appState.renderer.render(appState.camera, viewMatrix, projectionMatrix, currentTime);
        }
        {
            PROFILE_SCOPE("ImGui Render");
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        static double titleUpdateTimer = 0.0;
        titleUpdateTimer += appState.deltaSeconds;
        if (titleUpdateTimer > 0.5) {
            titleUpdateTimer = 0.0;
            char titleBuffer[256];
            snprintf(
                titleBuffer,
                sizeof(titleBuffer),
                "%s | R=%.0f | Ocean=%zu | %.1f FPS",
                appState.workflowStage == WorkflowStage::Render ? "Render" : "Procedural",
                appState.workflowStage == WorkflowStage::Render
                    ? appState.renderer.settings().planetRadius
                    : appState.proceduralSettings.planetRadius,
                appState.workflowStage == WorkflowStage::Render
                    ? appState.renderer.visibleOceanPatchCount()
                    : static_cast<std::size_t>(0),
                1.0f / glm::max(appState.deltaSeconds, 0.0001f)
            );
            glfwSetWindowTitle(window, titleBuffer);
        }

        {
            PROFILE_SCOPE("Swap Buffers");
            glfwSwapBuffers(window);
        }
        {
            PROFILE_SCOPE("Poll Events");
            glfwPollEvents();
        }

        if (profileEnabled && profileFrameLimit > 0 && ++profiledFrameCount >= profileFrameLimit) {
            glfwSetWindowShouldClose(window, true);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    if (profileEnabled) {
        Engine::Instrumentor::GetInstance().EndSession();
    }
    return 0;
}
