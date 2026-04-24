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
#include "PlanetRenderer.h"

namespace
{
constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

struct ApplicationState {
    FlyCamera camera{glm::vec3(0.0f, 9.0f, 42.0f)};
    PlanetRenderer renderer;
    bool firstMouseSample = true;
    float lastMouseX = kWindowWidth * 0.5f;
    float lastMouseY = kWindowHeight * 0.5f;
    float deltaSeconds = 0.0f;
    float previousFrameTime = 0.0f;
    bool showDebugPanel = true;
};

ApplicationState* getState(GLFWwindow* window)
{
    return static_cast<ApplicationState*>(glfwGetWindowUserPointer(window));
}

void onFramebufferSizeChanged(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}

void onMouseScrolled(GLFWwindow* window, double, double yOffset)
{
    ImGui_ImplGlfw_ScrollCallback(window, 0.0, yOffset);
    if (ImGui::GetIO().WantCaptureMouse) return;
    getState(window)->camera.zoom(static_cast<float>(yOffset) * 2.0f);
}

void onMouseMoved(GLFWwindow* window, double xPosition, double yPosition)
{
    ImGui_ImplGlfw_CursorPosCallback(window, xPosition, yPosition);

    ApplicationState* state = getState(window);
    if (ImGui::GetIO().WantCaptureMouse) {
        state->firstMouseSample = true;
        return;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
        state->firstMouseSample = true;
        return;
    }

    if (state->firstMouseSample) {
        state->lastMouseX = static_cast<float>(xPosition);
        state->lastMouseY = static_cast<float>(yPosition);
        state->firstMouseSample = false;
    }

    const float deltaX = static_cast<float>(xPosition) - state->lastMouseX;
    const float deltaY = state->lastMouseY - static_cast<float>(yPosition);
    state->lastMouseX = static_cast<float>(xPosition);
    state->lastMouseY = static_cast<float>(yPosition);

    state->camera.rotate(deltaX, deltaY);
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

    PlanetRenderSettings& settings = state->renderer.settings();

    if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
    if (key == GLFW_KEY_1) settings.renderMode = PlanetRenderMode::Shaded;
    if (key == GLFW_KEY_2) {
        settings.renderMode = PlanetRenderMode::Shaded;
        settings.showWireOverlay = !settings.showWireOverlay;
    }
    if (key == GLFW_KEY_3) settings.renderMode = PlanetRenderMode::HeightMap;
    if (key == GLFW_KEY_4) settings.renderMode = PlanetRenderMode::Normals;
    if (key == GLFW_KEY_T) settings.animateTerrain = !settings.animateTerrain;

    if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) {
        settings.tessellationMax = glm::min(settings.tessellationMax + 2.0f, 64.0f);
        std::cout << "Tessellation max: " << settings.tessellationMax << "\n";
    }

    if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) {
        settings.tessellationMax = glm::max(settings.tessellationMax - 2.0f, 1.0f);
        std::cout << "Tessellation max: " << settings.tessellationMax << "\n";
    }

    if (key == GLFW_KEY_TAB) {
        state->showDebugPanel = !state->showDebugPanel;
    }
}

void handleKeyboardMovement(GLFWwindow* window, ApplicationState& state)
{
    if (ImGui::GetIO().WantCaptureKeyboard) return;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) state.camera.move(FlyCamera::MovementDirection::Forward, state.deltaSeconds);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) state.camera.move(FlyCamera::MovementDirection::Backward, state.deltaSeconds);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) state.camera.move(FlyCamera::MovementDirection::Left, state.deltaSeconds);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) state.camera.move(FlyCamera::MovementDirection::Right, state.deltaSeconds);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) state.camera.move(FlyCamera::MovementDirection::Down, state.deltaSeconds);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) state.camera.move(FlyCamera::MovementDirection::Up, state.deltaSeconds);
}

void printControls(const PlanetRenderSettings& settings)
{
    std::cout << "\n=== Procedural Planet Controls ===\n";
    std::cout << "  W/A/S/D   : move camera\n";
    std::cout << "  Q/E       : move up/down\n";
    std::cout << "  RMB+drag  : look around\n";
    std::cout << "  Scroll    : zoom FOV\n";
    std::cout << "  1         : shaded mode\n";
    std::cout << "  2         : toggle wireframe overlay\n";
    std::cout << "  3         : height map mode\n";
    std::cout << "  4         : normal map mode\n";
    std::cout << "  +/-       : tessellation max level (current: " << settings.tessellationMax << ")\n";
    std::cout << "  T         : toggle animated terrain\n";
    std::cout << "  Tab       : toggle ImGui panel\n";
    std::cout << "  ESC       : quit\n\n";
}

void drawDebugPanel(ApplicationState& state)
{
    if (!state.showDebugPanel) return;

    PlanetRenderSettings& settings = state.renderer.settings();
    const float fps = 1.0f / glm::max(state.deltaSeconds, 0.0001f);
    int renderModeIndex = static_cast<int>(settings.renderMode);
    const char* renderModeNames[] = {"Shaded", "Unused", "Height Map", "Normals"};

    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 440.0f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Planet Controls", &state.showDebugPanel)) {
        ImGui::End();
        return;
    }

    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Visible patches: %zu", state.renderer.visiblePatchCount());
    ImGui::Separator();

    ImGui::Text("Planet Shape");
    ImGui::SliderFloat("Planet Radius", &settings.planetRadius, 5.0f, 80.0f, "%.1f");
    ImGui::SliderFloat("Height Scale", &settings.terrainHeightScale, 0.0f, 8.0f, "%.2f");
    ImGui::SliderFloat("Noise Scale", &settings.terrainNoiseScale, 0.2f, 10.0f, "%.2f");
    ImGui::Checkbox("Animate Terrain", &settings.animateTerrain);
    if (!settings.animateTerrain) {
        ImGui::SliderFloat("Animation Time", &settings.animationTime, 0.0f, 60.0f, "%.2f");
    }

    ImGui::Separator();
    ImGui::Text("Ocean Shell");
    ImGui::Checkbox("Render Ocean", &settings.renderOcean);
    ImGui::SliderFloat("Sea Level Offset", &settings.seaLevelOffset, -1.5f, 1.5f, "%.2f");
    ImGui::SliderFloat("Ocean Alpha", &settings.oceanAlpha, 0.05f, 1.0f, "%.2f");
    ImGui::SliderFloat("Ocean Fresnel", &settings.oceanFresnelStrength, 0.0f, 2.0f, "%.2f");

    ImGui::Separator();
    ImGui::Text("LOD and Tessellation");
    ImGui::SliderFloat("Tessellation Max", &settings.tessellationMax, 1.0f, 32.0f, "%.1f");
    ImGui::SliderFloat("Tessellation Min", &settings.tessellationMin, 1.0f, settings.tessellationMax, "%.1f");
    ImGui::SliderFloat("Tess Near", &settings.tessellationNearDistance, 1.0f, 60.0f, "%.1f");
    ImGui::SliderFloat("Tess Far", &settings.tessellationFarDistance, settings.tessellationNearDistance + 1.0f, 200.0f, "%.1f");
    ImGui::SliderFloat("Coarse Grid Width", &settings.coarseGridLineWidth, 0.5f, 5.0f, "%.2f");
    ImGui::Checkbox("Wire Overlay", &settings.showWireOverlay);

    ImGui::Separator();
    ImGui::Text("Render Mode");
    if (ImGui::Combo("Mode", &renderModeIndex, renderModeNames, IM_ARRAYSIZE(renderModeNames))) {
        if (renderModeIndex == 0) settings.renderMode = PlanetRenderMode::Shaded;
        if (renderModeIndex == 2) settings.renderMode = PlanetRenderMode::HeightMap;
        if (renderModeIndex == 3) settings.renderMode = PlanetRenderMode::Normals;
    }
    if (renderModeIndex == 1) {
        renderModeIndex = 0;
        settings.renderMode = PlanetRenderMode::Shaded;
    }

    ImGui::Separator();
    ImGui::Text("Camera");
    ImGui::SliderFloat("Move Speed", &state.camera.movementSpeed, 1.0f, 40.0f, "%.1f");
    ImGui::SliderFloat("Mouse Sensitivity", &state.camera.mouseSensitivity, 0.02f, 0.5f, "%.2f");
    ImGui::Text("Position: %.1f %.1f %.1f", state.camera.position.x, state.camera.position.y, state.camera.position.z);
    ImGui::Text("FOV: %.1f", state.camera.fieldOfView);

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

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL3_Init("#version 410");

    appState.renderer.initialize();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    printControls(appState.renderer.settings());

    while (!glfwWindowShouldClose(window))
    {
        const float currentTime = static_cast<float>(glfwGetTime());
        appState.deltaSeconds = currentTime - appState.previousFrameTime;
        appState.previousFrameTime = currentTime;

        if (appState.renderer.settings().animateTerrain) {
            appState.renderer.updateAnimationTime(currentTime);
        }

        handleKeyboardMovement(window, appState);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

        const glm::mat4 projectionMatrix = glm::perspective(
            glm::radians(appState.camera.fieldOfView),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            0.05f,
            500.0f
        );
        const glm::mat4 viewMatrix = appState.camera.viewMatrix();

        glClearColor(0.53f, 0.73f, 0.94f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawDebugPanel(appState);
        appState.renderer.render(appState.camera, viewMatrix, projectionMatrix);
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
                "Planet | R=%.0f | Patches=%zu | TessMax=%.0f | Mode=%s | %.1f FPS",
                appState.renderer.settings().planetRadius,
                appState.renderer.visiblePatchCount(),
                appState.renderer.settings().tessellationMax,
                appState.renderer.currentModeLabel(),
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
