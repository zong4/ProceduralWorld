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
#include <unordered_map>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "FlyCamera.h"
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
constexpr float kPlanetAutoSpinDegreesPerSecond = 3.0f;
constexpr const char* kSessionFilePath = "config/last_session.ini";
constexpr const char* kProceduralCacheFilePath = "config/last_procedural_cache.bin";
constexpr float kReferencePlanetRadius = 200.0f;
constexpr int kGenerationModuleCount = static_cast<int>(PlanetProceduralData::GenerationModule::Count);

enum class WorkflowStage {
    ProceduralSetup,
    Generating,
    Render
};

struct ApplicationState {
    FlyCamera camera{glm::vec3(0.0f, 90.0f, 420.0f)};
    PlanetRenderer renderer;
    PlanetRenderSettings proceduralSettings;
    PlanetRenderSettings renderSettings;
    PlanetProceduralData generatedPlanet;
    WorkflowStage workflowStage = WorkflowStage::ProceduralSetup;
    int generationFaceResolution = 128;
    float generationTimer = 0.0f;
    float generationDuration = 0.55f;
    PlanetRenderSettings pendingGenerationSettings;
    std::atomic<int> generationCompletedSteps{0};
    std::atomic<int> generationTotalSteps{1};
    std::atomic<int> generationActiveModule{0};
    std::array<std::atomic<int>, kGenerationModuleCount> generationModuleCompletedSteps{};
    std::array<std::atomic<int>, kGenerationModuleCount> generationModuleTotalSteps{};
    std::mutex generationStatusMutex;
    std::string generationStatusText = u8"\u51c6\u5907\u751f\u6210";
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

void copyProceduralSettings(PlanetRenderSettings& destination, const PlanetRenderSettings& source)
{
    destination.planetRadius = source.planetRadius;
    destination.seaLevelOffset = source.seaLevelOffset;
    destination.terrainHeightScale = source.terrainHeightScale;
    destination.terrainSkirtDepth = source.terrainSkirtDepth;
    destination.terrainNoiseScale = source.terrainNoiseScale;
    destination.mountainMaskStrength = source.mountainMaskStrength;
    destination.mountainMaskScale = source.mountainMaskScale;
    destination.mountainRidgeSharpness = source.mountainRidgeSharpness;
    destination.erosionIterations = source.erosionIterations;
    destination.erosionStrength = source.erosionStrength;
    destination.erosionTalus = source.erosionTalus;
    destination.erosionSediment = source.erosionSediment;
    destination.erosionThermalStrength = source.erosionThermalStrength;
    destination.regionalDetailStrength = source.regionalDetailStrength;
    destination.microDetailStrength = source.microDetailStrength;
    destination.regionalDetailStartAltitude = source.regionalDetailStartAltitude;
    destination.regionalDetailEndAltitude = source.regionalDetailEndAltitude;
    destination.microDetailStartAltitude = source.microDetailStartAltitude;
    destination.microDetailEndAltitude = source.microDetailEndAltitude;
    destination.terrainLowlandColor = source.terrainLowlandColor;
    destination.terrainForestColor = source.terrainForestColor;
    destination.terrainDesertColor = source.terrainDesertColor;
    destination.terrainRockColor = source.terrainRockColor;
    destination.terrainBeachColor = source.terrainBeachColor;
    destination.terrainSnowColor = source.terrainSnowColor;
    destination.terrainBeachWidth = source.terrainBeachWidth;
    destination.terrainShoreLift = source.terrainShoreLift;
    destination.terrainRockSlopeStart = source.terrainRockSlopeStart;
    destination.terrainRockSlopeEnd = source.terrainRockSlopeEnd;
    destination.terrainSnowStart = source.terrainSnowStart;
    destination.terrainSnowEnd = source.terrainSnowEnd;
    destination.terrainMaterialNoiseScale = source.terrainMaterialNoiseScale;
    destination.terrainMaterialNoiseStrength = source.terrainMaterialNoiseStrength;
}

float minCameraOrbitDistance(const PlanetRenderSettings& settings)
{
    return settings.planetRadius + glm::max(settings.terrainHeightScale, 1.0f) + 4.0f;
}

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
    if (state.cameraOrbitDistance <= 0.001f) {
        return;
    }

    const glm::vec3 direction = glm::normalize(state.camera.position);
    state.cameraOrbitPitchDegrees = wrapDegrees(glm::degrees(std::asin(glm::clamp(direction.y, -1.0f, 1.0f))));
    state.cameraOrbitYawDegrees = wrapDegrees(glm::degrees(std::atan2(direction.x, direction.z)));
}

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

void updateOrbitCamera(ApplicationState& state, const PlanetRenderSettings& settings)
{
    state.camera.fieldOfView = kLockedCameraFov;
    state.cameraOrbitDistance = glm::clamp(
        state.cameraOrbitDistance,
        minCameraOrbitDistance(settings),
        maxCameraOrbitDistance(settings)
    );

    glm::vec3 radialDirection = glm::length(state.camera.position) > 0.001f
        ? glm::normalize(state.camera.position)
        : glm::vec3(0.0f, 0.0f, 1.0f);
    state.camera.position = radialDirection * state.cameraOrbitDistance;
    orientCameraToPlanet(state, state.camera.up);
    updateOrbitMetadata(state);
}

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

#define PLANET_SETTING_FLOAT_FIELDS(X) \
    X(planetRadius) \
    X(seaLevelOffset) \
    X(tessellationMax) \
    X(tessellationMin) \
    X(tessellationNearDistance) \
    X(tessellationFarDistance) \
    X(oceanTessellationMax) \
    X(oceanTessellationMin) \
    X(oceanTessellationNearDistance) \
    X(oceanTessellationFarDistance) \
    X(terrainHeightScale) \
    X(terrainSkirtDepth) \
    X(terrainNoiseScale) \
    X(mountainMaskStrength) \
    X(mountainMaskScale) \
    X(mountainRidgeSharpness) \
    X(erosionStrength) \
    X(erosionTalus) \
    X(erosionSediment) \
    X(erosionThermalStrength) \
    X(regionalDetailStrength) \
    X(microDetailStrength) \
    X(regionalDetailStartAltitude) \
    X(regionalDetailEndAltitude) \
    X(microDetailStartAltitude) \
    X(microDetailEndAltitude) \
    X(terrainBeachWidth) \
    X(terrainShoreLift) \
    X(terrainRockSlopeStart) \
    X(terrainRockSlopeEnd) \
    X(terrainSnowStart) \
    X(terrainSnowEnd) \
    X(terrainMaterialNoiseScale) \
    X(terrainMaterialNoiseStrength) \
    X(coarseGridLineWidth) \
    X(fogDensity) \
    X(atmosphereHeight) \
    X(atmosphereDensity) \
    X(atmosphereRayleighStrength) \
    X(atmosphereMieStrength) \
    X(atmosphereMieAnisotropy) \
    X(atmosphereExposure) \
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
    X(oceanFftFrameStride) \
    X(terrainMaskDebugMode)

#define PLANET_SETTING_BOOL_FIELDS(X) \
    X(renderAtmosphere) \
    X(renderOceanReflectionRefraction) \
    X(renderOceanReflection) \
    X(renderOceanRefraction) \
    X(oceanAutoDistanceLod) \
    X(renderTerrain) \
    X(renderOcean)

#define PLANET_SETTING_VEC3_FIELDS(X) \
    X(terrainLowlandColor) \
    X(terrainForestColor) \
    X(terrainDesertColor) \
    X(terrainRockColor) \
    X(terrainBeachColor) \
    X(terrainSnowColor) \
    X(skyColor) \
    X(atmosphereRayleighColor) \
    X(atmosphereMieColor) \
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
        settings.wireMode = static_cast<PlanetWireMode>(glm::clamp(wireMode, 0, 2));
    }

    settings.erosionIterations = glm::clamp(settings.erosionIterations, 0, 512);
    settings.terrainMaskDebugMode = glm::clamp(settings.terrainMaskDebugMode, 0, 11);
    settings.oceanFftCascadeCount = glm::clamp(settings.oceanFftCascadeCount, 1, 3);
    settings.oceanFftFrameStride = glm::max(settings.oceanFftFrameStride, 1);
    settings.oceanReflectionFrameStride = glm::max(settings.oceanReflectionFrameStride, 1);
    settings.oceanRefractionFrameStride = glm::max(settings.oceanRefractionFrameStride, 1);
}

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
    writeVec3(file, "cameraUp", state.camera.up);
    writeFloat(file, "cameraOrbitDistance", state.cameraOrbitDistance);
    writeFloat(file, "cameraMouseSensitivity", state.camera.mouseSensitivity);
    writeBool(file, "showPerformancePanel", state.showPerformancePanel);
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
    state.generationFaceResolution = glm::clamp(state.generationFaceResolution, 32, 512);
    readFloat(values, "planetYawDegrees", state.planetYawDegrees);
    readFloat(values, "planetPitchDegrees", state.planetPitchDegrees);
    readVec3(values, "cameraPosition", state.camera.position);
    readVec3(values, "cameraUp", state.camera.up);
    readFloat(values, "cameraOrbitDistance", state.cameraOrbitDistance);
    readFloat(values, "cameraMouseSensitivity", state.camera.mouseSensitivity);
    readBool(values, "showPerformancePanel", state.showPerformancePanel);

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
    }

    const PlanetRenderSettings& activeSettings = state.workflowStage == WorkflowStage::Render
        ? state.renderer.settings()
        : state.renderSettings;
    if (glm::length(state.camera.position) <= 0.001f) {
        state.camera.position = glm::vec3(0.0f, 0.0f, activeSettings.planetRadius * 2.1f);
    }
    state.cameraOrbitDistance = glm::length(state.camera.position);
    updateOrbitCamera(state, activeSettings);

    state.sessionMessage = loadedCache
        ? std::string("Loaded session and cached planet: ") + path
        : std::string("Loaded session: ") + path;
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

float planetDistanceScale(float planetRadius)
{
    return glm::max(planetRadius / kReferencePlanetRadius, 0.25f);
}

const char* generationModuleLabel(int moduleIndex)
{
    static const char* kLabels[kGenerationModuleCount] = {
        u8"\u57fa\u7840\u9ad8\u5ea6",
        u8"\u4fb5\u8680\u6a21\u62df",
        u8"\u6c34\u6587\u6c14\u5019",
        u8"\u751f\u7269\u7fa4\u7cfb",
        u8"\u6536\u5c3e\u7edf\u8ba1"
    };
    if (moduleIndex < 0 || moduleIndex >= kGenerationModuleCount) {
        return "";
    }
    return kLabels[moduleIndex];
}

void startPlanetGeneration(ApplicationState& state)
{
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
        state.generationStatusText = u8"\u51c6\u5907\u751f\u6210";
    }

    PlanetRenderSettings generatedSettings = state.renderSettings;
    copyProceduralSettings(generatedSettings, state.proceduralSettings);
    state.pendingGenerationSettings = generatedSettings;
    const int faceResolution = state.generationFaceResolution;
    const int clampedResolution = glm::clamp(faceResolution, 16, 512);
    const int erosionIterations = glm::clamp(generatedSettings.erosionIterations, 0, 256);
    const float erosionStrength = glm::max(generatedSettings.erosionStrength, 0.0f);
    const float thermalStrength = glm::max(generatedSettings.erosionThermalStrength, 0.0f);
    const bool erosionActive = erosionIterations > 0 && (erosionStrength > 0.0f || thermalStrength > 0.0f);
    const int thermalIterations = erosionActive && thermalStrength > 0.0f
        ? glm::clamp(erosionIterations / 3, 1, 80)
        : 0;
    const int moduleTotals[kGenerationModuleCount] = {
        clampedResolution * 6,
        erosionActive ? erosionIterations + thermalIterations + 6 : 0,
        clampedResolution * 6 + 1,
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
                    *statusText = progress.status != nullptr ? progress.status : u8"\u751f\u6210\u4e2d";
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
    state.renderer.setProceduralData(state.generatedPlanet);
    state.renderSettings = generatedSettings;
    state.hasGeneratedPlanet = true;
    state.workflowStage = WorkflowStage::Render;

    state.camera.position = glm::vec3(0.0f, generatedSettings.planetRadius * 0.45f, generatedSettings.planetRadius * 2.10f);
    setOrbitFromCameraPosition(state, generatedSettings);
    saveSession(state);
}

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
    const float scrollStep = glm::max(state->cameraOrbitDistance * 0.09f, settings.planetRadius * 0.035f);
    state->cameraOrbitDistance -= static_cast<float>(yOffset) * scrollStep;
    updateOrbitCamera(*state, settings);
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

        state->planetYawDegrees += deltaX * 0.20f;
        state->planetPitchDegrees = glm::clamp(state->planetPitchDegrees + deltaY * 0.20f, -89.0f, 89.0f);
        state->renderer.setPlanetRotation(state->planetYawDegrees, state->planetPitchDegrees);
    } else {
        state->firstLeftMouseSample = true;
    }

    state->firstRightMouseSample = true;
}

void onMouseButtonChanged(GLFWwindow* window, int button, int action, int modifiers)
{
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, modifiers);
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

    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
    if (key == GLFW_KEY_1) {
        const int nextMode = (static_cast<int>(settings.renderMode) + 1) % 4;
        settings.renderMode = static_cast<PlanetRenderMode>(nextMode);
    }
    if (key == GLFW_KEY_2) {
        const int nextWireMode = (static_cast<int>(settings.wireMode) + 1) % 3;
        settings.wireMode = static_cast<PlanetWireMode>(nextWireMode);
    }
    if (key == GLFW_KEY_3) {
        settings.terrainMaskDebugMode = (settings.terrainMaskDebugMode + 1) % 12;
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

    const PlanetRenderSettings& settings = state.renderer.settings();
    const float orbitStep = kOrbitAngularSpeedDegrees * state.deltaSeconds;
    glm::vec3 newPosition = state.camera.position;
    glm::vec3 newUp = state.camera.up;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        newPosition = rotateVectorAroundAxis(newPosition, state.camera.up, -orbitStep);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        newPosition = rotateVectorAroundAxis(newPosition, state.camera.up, orbitStep);
    }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        newPosition = rotateVectorAroundAxis(newPosition, state.camera.right, -orbitStep);
        newUp = rotateVectorAroundAxis(newUp, state.camera.right, -orbitStep);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        newPosition = rotateVectorAroundAxis(newPosition, state.camera.right, orbitStep);
        newUp = rotateVectorAroundAxis(newUp, state.camera.right, orbitStep);
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        newUp = rotateVectorAroundAxis(newUp, state.camera.front, -orbitStep);
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        newUp = rotateVectorAroundAxis(newUp, state.camera.front, orbitStep);
    }

    state.camera.position = glm::normalize(newPosition) * state.cameraOrbitDistance;
    orientCameraToPlanet(state, newUp);
    updateOrbitMetadata(state);
}

void printControls()
{
    std::cout << "\n=== Procedural Planet Controls ===\n";
    std::cout << "  W/S       : orbit camera along screen up/down\n";
    std::cout << "  A/D       : orbit camera along screen left/right\n";
    std::cout << "  Q/E       : roll camera around view axis\n";
    std::cout << "  LMB+drag  : rotate planet\n";
    std::cout << "  Scroll    : dolly camera toward/away from planet\n";
    std::cout << "  1         : cycle render mode\n";
    std::cout << "  2         : cycle wire overlay\n";
    std::cout << "  3         : cycle terrain mask debug\n";
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
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), "Generating planet...");
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
            u8"\u6b63\u5728\u751f\u6210\uff1a%s - %s (%d/%d)",
            generationModuleLabel(activeModuleIndex),
            generationStatus.c_str(),
            activeModuleCompleted,
            activeModuleTotal
        );
        return;
        ImGui::Text(u8"\u6b63\u5728\u751f\u6210\uff1a%s (%d/%d)", generationStatus.c_str(), completedSteps, totalSteps);
        ImGui::Spacing();
        ImGui::TextDisabled(u8"\u6a21\u5757\u8fdb\u5ea6");

        const int activeModule = glm::clamp(
            state.generationActiveModule.load(std::memory_order_relaxed),
            0,
            kGenerationModuleCount - 1
        );
        for (int moduleIndex = 0; moduleIndex < kGenerationModuleCount; ++moduleIndex) {
            const int moduleCompleted = state.generationModuleCompletedSteps[static_cast<std::size_t>(moduleIndex)].load(std::memory_order_relaxed);
            const int moduleTotal = std::max(
                state.generationModuleTotalSteps[static_cast<std::size_t>(moduleIndex)].load(std::memory_order_relaxed),
                1
            );
            const float moduleProgress = glm::clamp(
                static_cast<float>(moduleCompleted) / static_cast<float>(moduleTotal),
                0.0f,
                1.0f
            );

            char overlay[32] = {};
            std::snprintf(overlay, sizeof(overlay), "%d/%d", moduleCompleted, moduleTotal);
            if (moduleIndex == activeModule) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.78f, 0.34f, 1.0f));
            }
            ImGui::Text("%s", generationModuleLabel(moduleIndex));
            if (moduleIndex == activeModule) {
                ImGui::PopStyleColor();
            }
            ImGui::ProgressBar(moduleProgress, ImVec2(-1.0f, 0.0f), overlay);
        }
        return;
        ImGui::Text(u8"正在生成... (%d/%d)", completedSteps, totalSteps);
        ImGui::TextDisabled("%s", generationStatus.c_str());
        return;
    }

    const float proceduralDistanceScale = planetDistanceScale(settings.planetRadius);
    ImGui::SliderFloat("Planet Radius", &settings.planetRadius, 50.0f, 5000.0f, "%.1f");
    ImGui::SliderFloat("Sea Level", &settings.seaLevelOffset, -1.5f, 1.5f, "%.2f");
    ImGui::SliderFloat("Height Scale", &settings.terrainHeightScale, 0.0f, 80.0f * proceduralDistanceScale, "%.2f");
    ImGui::SliderFloat("Noise Scale", &settings.terrainNoiseScale, 0.2f, 10.0f, "%.2f");
    ImGui::SliderFloat("Mountain Mask", &settings.mountainMaskStrength, 0.0f, 2.4f, "%.2f");
    ImGui::SliderFloat("Mountain Scale", &settings.mountainMaskScale, 0.5f, 8.0f, "%.2f");
    ImGui::SliderFloat("Ridge Sharpness", &settings.mountainRidgeSharpness, 1.0f, 6.0f, "%.2f");
    ImGui::SliderFloat("Skirt Depth", &settings.terrainSkirtDepth, 0.0f, 3.0f, "%.2f");
    if (ImGui::CollapsingHeader("Erosion Bake", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Iterations", &settings.erosionIterations, 0, 160);
        ImGui::SliderFloat("Strength", &settings.erosionStrength, 0.0f, 0.14f, "%.3f");
        ImGui::SliderFloat("Talus", &settings.erosionTalus, 0.005f, 0.12f, "%.3f");
        ImGui::SliderFloat("Sediment", &settings.erosionSediment, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Thermal", &settings.erosionThermalStrength, 0.0f, 0.08f, "%.3f");
    }
    ImGui::SliderFloat("Regional Detail", &settings.regionalDetailStrength, 0.0f, 1.2f, "%.2f");
    ImGui::SliderFloat("Micro Detail", &settings.microDetailStrength, 0.0f, 0.8f, "%.2f");
    ImGui::SliderInt("Face Resolution", &state.generationFaceResolution, 32, 512);

    if (ImGui::CollapsingHeader("Altitude Detail Bands")) {
        ImGui::TextDisabled("Distance scale: %.2fx from radius %.0f", proceduralDistanceScale, kReferencePlanetRadius);
        ImGui::SliderFloat("Regional Start", &settings.regionalDetailStartAltitude, 0.0f, 1800.0f * proceduralDistanceScale, "%.1f");
        ImGui::SliderFloat("Regional End", &settings.regionalDetailEndAltitude, settings.regionalDetailStartAltitude + 10.0f, 5000.0f * proceduralDistanceScale, "%.1f");
        ImGui::SliderFloat("Micro Start", &settings.microDetailStartAltitude, 0.0f, 800.0f * proceduralDistanceScale, "%.1f");
        ImGui::SliderFloat("Micro End", &settings.microDetailEndAltitude, settings.microDetailStartAltitude + 10.0f, 2000.0f * proceduralDistanceScale, "%.1f");
    }

    if (ImGui::CollapsingHeader("Terrain Materials")) {
        ImGui::ColorEdit3("Lowland Color", &settings.terrainLowlandColor.x);
        ImGui::ColorEdit3("Forest Color", &settings.terrainForestColor.x);
        ImGui::ColorEdit3("Desert Color", &settings.terrainDesertColor.x);
        ImGui::ColorEdit3("Rock Color", &settings.terrainRockColor.x);
        ImGui::ColorEdit3("Beach Color", &settings.terrainBeachColor.x);
        ImGui::ColorEdit3("Snow Color", &settings.terrainSnowColor.x);
        ImGui::SliderFloat("Beach Width", &settings.terrainBeachWidth, 0.005f, 0.20f, "%.3f");
        ImGui::SliderFloat("Shore Lift", &settings.terrainShoreLift, 0.0f, 0.12f, "%.3f");
        ImGui::SliderFloat("Rock Slope Start", &settings.terrainRockSlopeStart, 0.0f, 0.8f, "%.2f");
        ImGui::SliderFloat("Rock Slope End", &settings.terrainRockSlopeEnd, settings.terrainRockSlopeStart + 0.01f, 1.0f, "%.2f");
        ImGui::SliderFloat("Snow Start", &settings.terrainSnowStart, 0.2f, 1.0f, "%.2f");
        ImGui::SliderFloat("Snow End", &settings.terrainSnowEnd, settings.terrainSnowStart + 0.01f, 1.2f, "%.2f");
        ImGui::SliderFloat("Material Noise Scale", &settings.terrainMaterialNoiseScale, 0.002f, 0.20f, "%.3f");
        ImGui::SliderFloat("Material Noise", &settings.terrainMaterialNoiseStrength, 0.0f, 0.4f, "%.2f");
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
    static const char* kTerrainMaskDebugLabels[] = {
        "Off",
        "Channel",
        "Flow",
        "Wear",
        "Deposition",
        "Shore",
        "Water Depth",
        "Temperature",
        "Moisture",
        "Land",
        "Biome",
        "Hydrology RGB"
    };

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
    if (ImGui::RadioButton("Land Wire", wireModeIndex == 1)) settings.wireMode = PlanetWireMode::Terrain;
    ImGui::SameLine();
    if (ImGui::RadioButton("Ocean Wire", wireModeIndex == 2)) settings.wireMode = PlanetWireMode::Ocean;

    ImGui::Separator();
    ImGui::Text("Terrain Mask Debug");
    settings.terrainMaskDebugMode = glm::clamp(settings.terrainMaskDebugMode, 0, 11);
    if (ImGui::BeginCombo("Mask", kTerrainMaskDebugLabels[settings.terrainMaskDebugMode])) {
        for (int i = 0; i < 12; ++i) {
            const bool selected = settings.terrainMaskDebugMode == i;
            if (ImGui::Selectable(kTerrainMaskDebugLabels[i], selected)) {
                settings.terrainMaskDebugMode = i;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
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
        ImGui::Text("Water %.1f%% | Shore %.1f%% | Max depth %.2f",
                    state.generatedPlanet.waterCoverage() * 100.0f,
                    state.generatedPlanet.shoreCoverage() * 100.0f,
                    state.generatedPlanet.maxWaterDepth());
    }

    ImGui::Separator();
    drawRenderModeControls(settings);

    if (ImGui::CollapsingHeader("Visibility", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Render Terrain", &settings.renderTerrain);
        ImGui::SameLine();
        ImGui::Checkbox("Render Ocean", &settings.renderOcean);
        if (!settings.renderTerrain && !settings.renderOcean) {
            ImGui::TextDisabled("Both hidden");
        }
    }

    if (ImGui::CollapsingHeader("Scene Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Sky Color", &settings.skyColor.x);
        ImGui::SliderFloat("Fog Density", &settings.fogDensity, 0.0f, 0.00003f, "%.6f");
    }

    if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Render Atmosphere", &settings.renderAtmosphere);
        ImGui::SliderFloat("Atmosphere Height", &settings.atmosphereHeight, 1.0f, 120.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Atmosphere Density", &settings.atmosphereDensity, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Rayleigh", &settings.atmosphereRayleighStrength, 0.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("Mie", &settings.atmosphereMieStrength, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Mie G", &settings.atmosphereMieAnisotropy, 0.0f, 0.95f, "%.2f");
        ImGui::SliderFloat("Atmosphere Exposure", &settings.atmosphereExposure, 0.1f, 4.0f, "%.2f");
        ImGui::ColorEdit3("Rayleigh Color", &settings.atmosphereRayleighColor.x);
        ImGui::ColorEdit3("Mie Color", &settings.atmosphereMieColor.x);
    }

    if (ImGui::CollapsingHeader("Ocean", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Opacity Limit", &settings.oceanAlpha, 0.05f, 1.0f, "%.2f");
        ImGui::ColorEdit3("Shallow Color", &settings.oceanShallowColor.x);
        ImGui::ColorEdit3("Deep Color", &settings.oceanDeepColor.x);
        ImGui::ColorEdit3("SSS Color", &settings.oceanSSSColor.x);
    }

    if (ImGui::CollapsingHeader("Ocean Waves", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Wave Height", &settings.oceanWaveAmplitude, 0.0f, 0.80f, "%.3f");
        ImGui::SliderFloat("Choppiness", &settings.oceanChoppiness, 0.0f, 0.80f, "%.3f");
        ImGui::SliderFloat("Wave Tile Scale", &settings.oceanWaveTileScale, 4.0f, 28.0f, "%.1f");
        ImGui::SliderFloat("FFT Normal", &settings.oceanWaveNormalStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderInt("FFT Cascades", &settings.oceanFftCascadeCount, 1, 3);
        ImGui::SliderInt("FFT Frame Stride", &settings.oceanFftFrameStride, 1, 8);
    }

    if (ImGui::CollapsingHeader("Ocean Material", ImGuiTreeNodeFlags_DefaultOpen)) {
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
    }

    if (ImGui::CollapsingHeader("Ocean Reflection", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Planar Targets", &settings.renderOceanReflectionRefraction);
        ImGui::Checkbox("Reflection", &settings.renderOceanReflection);
        ImGui::Checkbox("Refraction", &settings.renderOceanRefraction);
        ImGui::SliderFloat("Target Scale", &settings.oceanReflectionResolutionScale, 0.25f, 1.0f, "%.2f");
        ImGui::SliderInt("Reflection Stride", &settings.oceanReflectionFrameStride, 1, 8);
        ImGui::SliderInt("Refraction Stride", &settings.oceanRefractionFrameStride, 1, 8);
        ImGui::Checkbox("Auto Distance LOD", &settings.oceanAutoDistanceLod);
        ImGui::SliderFloat("Reflection Max Alt", &settings.oceanReflectionMaxAltitude, 40.0f, 2000.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Refraction Max Alt", &settings.oceanRefractionMaxAltitude, 10.0f, 800.0f * renderDistanceScale, "%.1f");
    }

    if (ImGui::CollapsingHeader("Rendering Advanced")) {
        ImGui::TextDisabled("Distance scale: %.2fx from radius %.0f", renderDistanceScale, kReferencePlanetRadius);
        ImGui::SliderFloat("Land Tess Max", &settings.tessellationMax, 1.0f, 4.0f, "%.1f");
        ImGui::SliderFloat("Land Tess Min", &settings.tessellationMin, 1.0f, settings.tessellationMax, "%.1f");
        ImGui::SliderFloat("Land Tess Near", &settings.tessellationNearDistance, 10.0f, 600.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Land Tess Far", &settings.tessellationFarDistance, settings.tessellationNearDistance + 10.0f, 2000.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Ocean Tess Max", &settings.oceanTessellationMax, 1.0f, 4.0f, "%.1f");
        ImGui::SliderFloat("Ocean Tess Min", &settings.oceanTessellationMin, 1.0f, settings.oceanTessellationMax, "%.1f");
        ImGui::SliderFloat("Ocean Tess Near", &settings.oceanTessellationNearDistance, 10.0f, 400.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Ocean Tess Far", &settings.oceanTessellationFarDistance, settings.oceanTessellationNearDistance + 10.0f, 1400.0f * renderDistanceScale, "%.1f");
        ImGui::SliderFloat("Near Plane", &settings.cameraNearPlane, 0.20f, 5.0f, "%.2f");
        ImGui::SliderFloat("Far Plane", &settings.cameraFarPlane, 1000.0f, 12000.0f * renderDistanceScale, "%.0f");
    }

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::SliderFloat("Orbit Distance", &state.cameraOrbitDistance, minCameraOrbitDistance(settings), maxCameraOrbitDistance(settings), "%.1f");
    ImGui::Text("Yaw %.1f | Pitch %.1f", state.cameraOrbitYawDegrees, state.cameraOrbitPitchDegrees);
    ImGui::SliderFloat("Mouse Sensitivity", &state.camera.mouseSensitivity, 0.02f, 0.5f, "%.2f");
    ImGui::Text("Position: %.1f %.1f %.1f", state.camera.position.x, state.camera.position.y, state.camera.position.z);
    ImGui::Text("Altitude: %.1f", glm::max(glm::length(state.camera.position) - settings.planetRadius, 0.0f));
    ImGui::Text("FOV: %.1f", state.camera.fieldOfView);

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
    ImGui::SetNextWindowSize(ImVec2(360.0f, 280.0f), ImGuiCond_FirstUseEver);

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

    if (ImGui::Begin("Performance Monitor  Ctrl+1", &state.showPerformancePanel)) {
        const float fps = 1.0f / glm::max(state.deltaSeconds, 0.0001f);
        const PlanetRenderer::PerformanceStats& perf = state.renderer.performanceStats();
        const PlanetRenderer::CullingStats& cullingStats = state.renderer.cullingStats();

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
        ImGui::Text("Visible patches: %zu", state.renderer.visiblePatchCount());
        ImGui::Text("Ocean patches: %zu / %zu", perf.oceanPatchCount, state.renderer.visiblePatchCount());
        ImGui::Text("LOD nodes: %zu | Split: %zu", cullingStats.visitedNodes, cullingStats.splitNodes);
        ImGui::Text("Culled: %zu frustum | %zu horizon", cullingStats.frustumCulledNodes, cullingStats.horizonCulledNodes);

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
    ImGui::SetNextWindowSize(ImVec2(380.0f, 520.0f), ImGuiCond_FirstUseEver);

    const char* title = state.workflowStage == WorkflowStage::Render
        ? "Render Controls"
        : "Procedural Generation";

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

    if (!ImGui::Begin(title, &state.showDebugPanel)) {
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

    io.Fonts->AddFontDefault();
}
} // namespace

int main()
{
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
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
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

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
        return -1;
    }

    GLint maxPatchVertices = 0;
    glGetIntegerv(GL_MAX_PATCH_VERTICES, &maxPatchVertices);
    std::cout << "GL_MAX_PATCH_VERTICES = " << maxPatchVertices << "\n";
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "OpenGL vendor: " << glGetString(GL_VENDOR) << "\n";
    std::cout << "OpenGL renderer: " << glGetString(GL_RENDERER) << "\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    configureImGuiFonts();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init("#version 410");

    appState.renderer.initialize();
    appState.renderSettings = appState.renderer.settings();
    appState.proceduralSettings = appState.renderer.settings();
    loadSession(appState, kSessionFilePath, false);
    appState.renderer.setPlanetRotation(appState.planetYawDegrees, appState.planetPitchDegrees);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    printControls();

    while (!glfwWindowShouldClose(window))
    {
        const float currentTime = static_cast<float>(glfwGetTime());
        appState.deltaSeconds = currentTime - appState.previousFrameTime;
        appState.previousFrameTime = currentTime;

        if (appState.workflowStage == WorkflowStage::Generating) {
            appState.generationTimer += appState.deltaSeconds;
            if (appState.generationFuture.valid()
                && appState.generationFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                try {
                    finishPlanetGeneration(appState, appState.generationFuture.get());
                } catch (const std::exception& exception) {
                    appState.workflowStage = WorkflowStage::ProceduralSetup;
                    appState.sessionMessage = std::string("Planet generation failed: ") + exception.what();
                }
            }
        }

        handleKeyboardMovement(window, appState);
        if (appState.workflowStage == WorkflowStage::Render) {
            appState.planetYawDegrees += kPlanetAutoSpinDegreesPerSecond * appState.deltaSeconds;
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

        const glm::mat4 projectionMatrix = glm::perspective(
            glm::radians(appState.camera.fieldOfView),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            activeSettings.cameraNearPlane,
            activeSettings.cameraFarPlane
        );
        const glm::mat4 viewMatrix = appState.camera.viewMatrix();

        const glm::vec3 skyColor = appState.workflowStage == WorkflowStage::Render
            ? appState.renderer.settings().skyColor
            : glm::vec3(0.0f);
        glClearColor(skyColor.r, skyColor.g, skyColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawDebugPanel(appState);
        drawPerformancePanel(appState);
        if (appState.workflowStage == WorkflowStage::Render) {
            appState.renderer.render(appState.camera, viewMatrix, projectionMatrix, currentTime);
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        static double titleUpdateTimer = 0.0;
        titleUpdateTimer += appState.deltaSeconds;
        if (titleUpdateTimer > 0.5) {
            titleUpdateTimer = 0.0;
            char titleBuffer[256];
            snprintf(
                titleBuffer,
                sizeof(titleBuffer),
                "%s | R=%.0f | Patches=%zu | %.1f FPS",
                appState.workflowStage == WorkflowStage::Render ? "Render" : "Procedural",
                appState.workflowStage == WorkflowStage::Render
                    ? appState.renderer.settings().planetRadius
                    : appState.proceduralSettings.planetRadius,
                appState.workflowStage == WorkflowStage::Render
                    ? appState.renderer.visiblePatchCount()
                    : static_cast<std::size_t>(0),
                1.0f / glm::max(appState.deltaSeconds, 0.0001f)
            );
            glfwSetWindowTitle(window, titleBuffer);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
    return 0;
}
