#include <cstdio>
#include <iostream>

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
    float planetYawDegrees = 0.0f;
    float planetPitchDegrees = 0.0f;
    float cameraOrbitYawDegrees = 0.0f;
    float cameraOrbitPitchDegrees = 12.0f;
    float cameraOrbitDistance = 420.0f;
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
    destination.terrainNoiseScale = source.terrainNoiseScale;
    destination.mountainMaskStrength = source.mountainMaskStrength;
    destination.mountainMaskScale = source.mountainMaskScale;
    destination.mountainRidgeSharpness = source.mountainRidgeSharpness;
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

void updateOrbitCamera(ApplicationState& state, const PlanetRenderSettings& settings)
{
    state.camera.fieldOfView = kLockedCameraFov;
    state.cameraOrbitPitchDegrees = glm::clamp(state.cameraOrbitPitchDegrees, -82.0f, 82.0f);
    state.cameraOrbitDistance = glm::clamp(
        state.cameraOrbitDistance,
        minCameraOrbitDistance(settings),
        maxCameraOrbitDistance(settings)
    );

    const float yaw = glm::radians(state.cameraOrbitYawDegrees);
    const float pitch = glm::radians(state.cameraOrbitPitchDegrees);
    const float cp = glm::cos(pitch);
    state.camera.position = glm::vec3(
        glm::sin(yaw) * cp,
        glm::sin(pitch),
        glm::cos(yaw) * cp
    ) * state.cameraOrbitDistance;
    state.camera.lookAt(glm::vec3(0.0f));
}

void setOrbitFromCameraPosition(ApplicationState& state, const PlanetRenderSettings& settings)
{
    state.cameraOrbitDistance = glm::clamp(
        glm::length(state.camera.position),
        minCameraOrbitDistance(settings),
        maxCameraOrbitDistance(settings)
    );
    if (state.cameraOrbitDistance > 0.001f) {
        const glm::vec3 direction = glm::normalize(state.camera.position);
        state.cameraOrbitPitchDegrees = glm::degrees(glm::asin(glm::clamp(direction.y, -1.0f, 1.0f)));
        state.cameraOrbitYawDegrees = glm::degrees(glm::atan(direction.x, direction.z));
    }
    updateOrbitCamera(state, settings);
}

void startPlanetGeneration(ApplicationState& state)
{
    state.workflowStage = WorkflowStage::Generating;
    state.generationTimer = 0.0f;
}

void finishPlanetGeneration(ApplicationState& state)
{
    PlanetRenderSettings generatedSettings = state.renderSettings;
    copyProceduralSettings(generatedSettings, state.proceduralSettings);
    state.generatedPlanet.generate(generatedSettings, state.generationFaceResolution);
    state.renderer.settings() = generatedSettings;
    state.renderer.setProceduralData(state.generatedPlanet);
    state.renderSettings = generatedSettings;
    state.hasGeneratedPlanet = true;
    state.workflowStage = WorkflowStage::Render;

    state.camera.position = glm::vec3(0.0f, generatedSettings.planetRadius * 0.45f, generatedSettings.planetRadius * 2.10f);
    setOrbitFromCameraPosition(state, generatedSettings);
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

void onKeyPressed(GLFWwindow* window, int key, int, int action, int)
{
    ImGui_ImplGlfw_KeyCallback(window, key, 0, action, 0);
    if (action != GLFW_PRESS) return;

    ApplicationState* state = getState(window);
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
    const float distanceStep = glm::max(state.cameraOrbitDistance * 0.90f, settings.planetRadius * 0.40f) * state.deltaSeconds;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) state.cameraOrbitYawDegrees -= orbitStep;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) state.cameraOrbitYawDegrees += orbitStep;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) state.cameraOrbitDistance -= distanceStep;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) state.cameraOrbitDistance += distanceStep;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) state.cameraOrbitPitchDegrees -= orbitStep;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) state.cameraOrbitPitchDegrees += orbitStep;

    updateOrbitCamera(state, settings);
}

void printControls()
{
    std::cout << "\n=== Procedural Planet Controls ===\n";
    std::cout << "  W/S       : dolly camera toward/away from planet\n";
    std::cout << "  A/D       : orbit camera around planet\n";
    std::cout << "  Q/E       : orbit camera latitude\n";
    std::cout << "  LMB+drag  : rotate planet\n";
    std::cout << "  Scroll    : dolly camera toward/away from planet\n";
    std::cout << "  1         : cycle render mode\n";
    std::cout << "  2         : cycle wire overlay\n";
    std::cout << "  Tab       : toggle ImGui panel\n";
    std::cout << "  ESC       : quit\n\n";
}

void drawProceduralPanel(ApplicationState& state)
{
    PlanetRenderSettings& settings = state.proceduralSettings;

    ImGui::Text("Procedural Generation");
    ImGui::Separator();

    if (state.workflowStage == WorkflowStage::Generating) {
        const float progress = glm::clamp(state.generationTimer / glm::max(state.generationDuration, 0.001f), 0.0f, 1.0f);
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), "Generating planet...");
        return;
    }

    ImGui::SliderFloat("Planet Radius", &settings.planetRadius, 50.0f, 800.0f, "%.1f");
    ImGui::SliderFloat("Sea Level", &settings.seaLevelOffset, -1.5f, 1.5f, "%.2f");
    ImGui::SliderFloat("Height Scale", &settings.terrainHeightScale, 0.0f, 80.0f, "%.2f");
    ImGui::SliderFloat("Noise Scale", &settings.terrainNoiseScale, 0.2f, 10.0f, "%.2f");
    ImGui::SliderFloat("Mountain Mask", &settings.mountainMaskStrength, 0.0f, 1.5f, "%.2f");
    ImGui::SliderFloat("Mountain Scale", &settings.mountainMaskScale, 0.5f, 8.0f, "%.2f");
    ImGui::SliderFloat("Ridge Sharpness", &settings.mountainRidgeSharpness, 1.0f, 6.0f, "%.2f");
    ImGui::SliderFloat("Regional Detail", &settings.regionalDetailStrength, 0.0f, 1.2f, "%.2f");
    ImGui::SliderFloat("Micro Detail", &settings.microDetailStrength, 0.0f, 0.8f, "%.2f");
    ImGui::SliderInt("Face Resolution", &state.generationFaceResolution, 32, 256);

    if (ImGui::CollapsingHeader("Altitude Detail Bands")) {
        ImGui::SliderFloat("Regional Start", &settings.regionalDetailStartAltitude, 0.0f, 600.0f, "%.1f");
        ImGui::SliderFloat("Regional End", &settings.regionalDetailEndAltitude, settings.regionalDetailStartAltitude + 10.0f, 2400.0f, "%.1f");
        ImGui::SliderFloat("Micro Start", &settings.microDetailStartAltitude, 0.0f, 300.0f, "%.1f");
        ImGui::SliderFloat("Micro End", &settings.microDetailEndAltitude, settings.microDetailStartAltitude + 10.0f, 1200.0f, "%.1f");
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
}

void drawRenderPanel(ApplicationState& state)
{
    PlanetRenderSettings& settings = state.renderer.settings();
    const float fps = 1.0f / glm::max(state.deltaSeconds, 0.0001f);

    if (ImGui::Button("Back To Procedural", ImVec2(-1.0f, 28.0f))) {
        returnToProceduralSetup(state);
        return;
    }

    ImGui::Separator();
    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Visible patches: %zu", state.renderer.visiblePatchCount());
    if (state.generatedPlanet.isGenerated()) {
        ImGui::Text("Generated data: %d face resolution", state.generatedPlanet.resolution());
        ImGui::Text("Water %.1f%% | Shore %.1f%% | Max depth %.2f",
                    state.generatedPlanet.waterCoverage() * 100.0f,
                    state.generatedPlanet.shoreCoverage() * 100.0f,
                    state.generatedPlanet.maxWaterDepth());
    }
    const PlanetRenderer::CullingStats& cullingStats = state.renderer.cullingStats();
    ImGui::Text("LOD nodes: %zu | Split: %zu", cullingStats.visitedNodes, cullingStats.splitNodes);
    ImGui::Text("Culled: %zu frustum | %zu horizon", cullingStats.frustumCulledNodes, cullingStats.horizonCulledNodes);

    if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen)) {
        const PlanetRenderer::PerformanceStats& perf = state.renderer.performanceStats();
        ImGui::Text("CPU submit timings, not blocking GPU");
        ImGui::Text("Total: %.2f ms", perf.totalMs);
        ImGui::Text("Culling: %.2f | FFT: %.2f %s", perf.cullingMs, perf.fftMs, perf.fftUpdated ? "(updated)" : "(reused)");
        ImGui::Text("FFT cascades: %d | stride: %d frame(s)", perf.fftCascadeCount, perf.fftFrameStride);
        ImGui::Text("Reflection/Refraction: %.2f", perf.reflectionRefractionMs);
        ImGui::Text("Reflection: %s | Refraction: %s",
                    perf.reflectionEnabled ? (perf.reflectionUpdated ? "updated" : "reused") : "off",
                    perf.refractionEnabled ? (perf.refractionUpdated ? "updated" : "reused") : "off");
        ImGui::Text("Refl weight: %.2f | Refr weight: %.2f", perf.reflectionWeight, perf.refractionWeight);
        ImGui::Text("Ocean patches: %zu / %zu", perf.oceanPatchCount, state.renderer.visiblePatchCount());
        ImGui::Text("Terrain: %.2f | Ocean: %.2f", perf.terrainMs, perf.oceanMs);
        ImGui::Text("Atmosphere: %.2f | Wire: %.2f", perf.atmosphereMs, perf.wireMs);
    }

    ImGui::Separator();
    drawRenderModeControls(settings);

    if (ImGui::CollapsingHeader("Scene Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Sky Color", &settings.skyColor.x);
        ImGui::SliderFloat("Fog Density", &settings.fogDensity, 0.0f, 0.00003f, "%.6f");
    }

    if (ImGui::CollapsingHeader("Atmosphere", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Render Atmosphere", &settings.renderAtmosphere);
        ImGui::SliderFloat("Atmosphere Height", &settings.atmosphereHeight, 1.0f, 120.0f, "%.1f");
        ImGui::SliderFloat("Atmosphere Density", &settings.atmosphereDensity, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Rayleigh", &settings.atmosphereRayleighStrength, 0.0f, 4.0f, "%.2f");
        ImGui::SliderFloat("Mie", &settings.atmosphereMieStrength, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Mie G", &settings.atmosphereMieAnisotropy, 0.0f, 0.95f, "%.2f");
        ImGui::SliderFloat("Atmosphere Exposure", &settings.atmosphereExposure, 0.1f, 4.0f, "%.2f");
        ImGui::ColorEdit3("Rayleigh Color", &settings.atmosphereRayleighColor.x);
        ImGui::ColorEdit3("Mie Color", &settings.atmosphereMieColor.x);
    }

    if (ImGui::CollapsingHeader("Ocean", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Render Ocean", &settings.renderOcean);
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
        ImGui::SliderFloat("Detail Fade", &settings.oceanDetailFadeDistance, 40.0f, 1200.0f, "%.1f");
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
        ImGui::SliderFloat("Reflection Max Alt", &settings.oceanReflectionMaxAltitude, 40.0f, 2000.0f, "%.1f");
        ImGui::SliderFloat("Refraction Max Alt", &settings.oceanRefractionMaxAltitude, 10.0f, 800.0f, "%.1f");
    }

    if (ImGui::CollapsingHeader("Rendering Advanced")) {
        ImGui::SliderFloat("Land Tess Max", &settings.tessellationMax, 1.0f, 4.0f, "%.1f");
        ImGui::SliderFloat("Land Tess Min", &settings.tessellationMin, 1.0f, settings.tessellationMax, "%.1f");
        ImGui::SliderFloat("Land Tess Near", &settings.tessellationNearDistance, 10.0f, 600.0f, "%.1f");
        ImGui::SliderFloat("Land Tess Far", &settings.tessellationFarDistance, settings.tessellationNearDistance + 10.0f, 2000.0f, "%.1f");
        ImGui::SliderFloat("Ocean Tess Max", &settings.oceanTessellationMax, 1.0f, 4.0f, "%.1f");
        ImGui::SliderFloat("Ocean Tess Min", &settings.oceanTessellationMin, 1.0f, settings.oceanTessellationMax, "%.1f");
        ImGui::SliderFloat("Ocean Tess Near", &settings.oceanTessellationNearDistance, 10.0f, 400.0f, "%.1f");
        ImGui::SliderFloat("Ocean Tess Far", &settings.oceanTessellationFarDistance, settings.oceanTessellationNearDistance + 10.0f, 1400.0f, "%.1f");
        ImGui::SliderFloat("Near Plane", &settings.cameraNearPlane, 0.02f, 2.0f, "%.2f");
        ImGui::SliderFloat("Far Plane", &settings.cameraFarPlane, 1000.0f, 12000.0f, "%.0f");
    }

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::SliderFloat("Orbit Distance", &state.cameraOrbitDistance, minCameraOrbitDistance(settings), maxCameraOrbitDistance(settings), "%.1f");
    ImGui::SliderFloat("Orbit Pitch", &state.cameraOrbitPitchDegrees, -82.0f, 82.0f, "%.1f");
    ImGui::SliderFloat("Mouse Sensitivity", &state.camera.mouseSensitivity, 0.02f, 0.5f, "%.2f");
    ImGui::Text("Position: %.1f %.1f %.1f", state.camera.position.x, state.camera.position.y, state.camera.position.z);
    ImGui::Text("Altitude: %.1f", glm::max(glm::length(state.camera.position) - settings.planetRadius, 0.0f));
    ImGui::Text("FOV: %.1f", state.camera.fieldOfView);

    state.renderSettings = settings;
}

void drawDebugPanel(ApplicationState& state)
{
    if (!state.showDebugPanel) return;

    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380.0f, 520.0f), ImGuiCond_FirstUseEver);

    const char* title = state.workflowStage == WorkflowStage::Render
        ? "Render Controls"
        : "Procedural Generation";

    if (!ImGui::Begin(title, &state.showDebugPanel)) {
        ImGui::End();
        return;
    }

    if (state.workflowStage == WorkflowStage::Render) {
        drawRenderPanel(state);
    } else {
        drawProceduralPanel(state);
    }

    ImGui::End();
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
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init("#version 410");

    appState.renderer.initialize();
    appState.renderSettings = appState.renderer.settings();
    appState.proceduralSettings = appState.renderer.settings();
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
            if (appState.generationTimer >= appState.generationDuration) {
                finishPlanetGeneration(appState);
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
