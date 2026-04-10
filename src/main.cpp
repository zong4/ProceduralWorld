/**
 * Terrain Renderer — Advanced Graphics Assignment
 *
 * Features:
 *  - Realistic terrain geometry via domain-warped fBm noise (Perlin gradient noise)
 *  - Variable-resolution rendering via OpenGL Tessellation Shaders (TCS + TES)
 *  - Distance-based adaptive tessellation (near = high detail, far = low detail)
 *  - Multiple render modes: shaded / wireframe overlay / height map / normals
 *  - Atmospheric fog, hemisphere ambient, Blinn-Phong lighting, biome coloring
 *
 * Controls:
 *   WASD / QE  — fly camera
 *   Mouse drag — look around
 *   Scroll     — FOV zoom
 *   1/2/3/4    — render mode (shaded / wireframe / heightmap / normals)
 *   +/-        — increase/decrease tessellation max level
 *   T          — toggle dynamic time
 *   ESC        — quit
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <cmath>

#include "Shader.h"
#include "Camera.h"

// ------------------------------------------------------------------ settings
constexpr int   SCR_W      = 1280;
constexpr int   SCR_H      = 720;
constexpr int   GRID_N     = 64;   // patches per side (total = GRID_N*GRID_N quads)

enum class RenderMode : int {
    Shaded    = 0,
    HeightMap = 2,
    Normals   = 3
};

struct RenderSettings {
    float tessMax         = 32.0f;
    float tessMin         = 1.0f;
    float tessMinDist     = 2.0f;
    float tessMaxDist     = 30.0f;
    float heightScale     = 2.2f;
    float noiseScale      = 3.5f;
    float wireCoarseWidth = 1.6f;
    RenderMode mode       = RenderMode::Shaded;
    bool wireOverlay      = false;
    bool animTime         = false;
    float appTime         = 0.0f;
};

// ------------------------------------------------------------------ globals
Camera camera(glm::vec3(0.5f, 4.5f, 1.2f));
bool   firstMouse = true;
float  lastX = SCR_W / 2.0f, lastY = SCR_H / 2.0f;
float  deltaTime = 0.0f, lastFrame = 0.0f;
RenderSettings settings;

GLFWwindow* window = nullptr;

// ------------------------------------------------------------------ callbacks
void framebufferSizeCB(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }

void scrollCB(GLFWwindow*, double, double yoff) { camera.zoom((float)yoff * 2.0f); }

void keyCB(GLFWwindow* win, int key, int, int action, int)
{
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE)     glfwSetWindowShouldClose(win, true);
        if (key == GLFW_KEY_1)          settings.mode = RenderMode::Shaded;
        if (key == GLFW_KEY_2)          { settings.mode = RenderMode::Shaded; settings.wireOverlay = !settings.wireOverlay; }
        if (key == GLFW_KEY_3)          settings.mode = RenderMode::HeightMap;
        if (key == GLFW_KEY_4)          settings.mode = RenderMode::Normals;
        if (key == GLFW_KEY_T)          settings.animTime = !settings.animTime;
        if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) {
            settings.tessMax = glm::min(settings.tessMax + 4.0f, 64.0f);
            std::cout << "Tess max: " << settings.tessMax << "\n";
        }
        if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) {
            settings.tessMax = glm::max(settings.tessMax - 4.0f, 1.0f);
            std::cout << "Tess max: " << settings.tessMax << "\n";
        }
    }
}

void mouseCB(GLFWwindow*, double xpos, double ypos)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
        firstMouse = true;
        return;
    }
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
    float dx = (float)xpos - lastX, dy = lastY - (float)ypos;
    lastX = (float)xpos; lastY = (float)ypos;
    camera.rotate(dx, dy);
}

// ------------------------------------------------------------------ terrain mesh
struct TerrainMesh {
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLsizei indexCount = 0;

    void build(int N)
    {
        // Build a uniform grid of (N+1)x(N+1) vertices in [0,1]x[0,1]
        // Each quad = one quad-patch (4 vertices) for the tessellator.
        // We use indexed GL_PATCHES with patch size = 4.
        std::vector<float>    verts;
        std::vector<unsigned> indices;

        float step = 1.0f / N;

        for (int z = 0; z <= N; ++z)
            for (int x = 0; x <= N; ++x) {
                verts.push_back(x * step);   // u
                verts.push_back(z * step);   // v
            }

        // Each quad: 4 corner indices (CCW from bottom-left)
        for (int z = 0; z < N; ++z) {
            for (int x = 0; x < N; ++x) {
                unsigned bl = z       * (N+1) + x;
                unsigned br = z       * (N+1) + x + 1;
                unsigned tl = (z + 1) * (N+1) + x;
                unsigned tr = (z + 1) * (N+1) + x + 1;
                // quad patch order: BL BR TR TL (matching tesc/tese expectations)
                indices.push_back(bl);
                indices.push_back(br);
                indices.push_back(tr);
                indices.push_back(tl);
            }
        }
        indexCount = (GLsizei)indices.size();

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned), indices.data(), GL_STATIC_DRAW);

        // Attribute 0: vec2 (u, v)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }

    void draw() const
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_PATCHES, indexCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }
};

void applyCommonUniforms(Shader& shader,
                         const glm::mat4& model,
                         const glm::mat4& view,
                         const glm::mat4& projection,
                         const glm::vec3& lightDir)
{
    shader.use();
    shader.setMat4("model", model);
    shader.setMat4("view", view);
    shader.setMat4("projection", projection);
    shader.setVec3("cameraPos", camera.Position);
    shader.setVec3("lightDir", lightDir);
    shader.setFloat("time", settings.appTime);
    shader.setFloat("tessMin", settings.tessMin);
    shader.setFloat("tessMax", settings.tessMax);
    shader.setFloat("tessMinDist", settings.tessMinDist);
    shader.setFloat("tessMaxDist", settings.tessMaxDist);
    shader.setFloat("heightScale", settings.heightScale);
    shader.setFloat("noiseScale", settings.noiseScale);
    shader.setFloat("gridCount", (float)GRID_N);
    shader.setFloat("coarseLineWidth", settings.wireCoarseWidth);
    shader.setInt("renderMode", (int)settings.mode);
}

void handleMovementInput()
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.move(Camera::Dir::FORWARD,  deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.move(Camera::Dir::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.move(Camera::Dir::LEFT,     deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.move(Camera::Dir::RIGHT,    deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camera.move(Camera::Dir::DOWN,     deltaTime);
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) camera.move(Camera::Dir::UP,       deltaTime);
}

void drawTerrain(const TerrainMesh& terrain,
                 Shader& terrainShader,
                 const glm::mat4& model,
                 const glm::mat4& view,
                 const glm::mat4& projection,
                 const glm::vec3& lightDir)
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    applyCommonUniforms(terrainShader, model, view, projection, lightDir);
    terrain.draw();
}

void drawWireOverlay(const TerrainMesh& terrain,
                     Shader& wireShader,
                     Shader& coarseGridShader,
                     const glm::mat4& model,
                     const glm::mat4& view,
                     const glm::mat4& projection,
                     const glm::vec3& lightDir)
{
    if (!settings.wireOverlay) return;

    glEnable(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(-1.0f, -1.0f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    applyCommonUniforms(wireShader, model, view, projection, lightDir);
    terrain.draw();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    applyCommonUniforms(coarseGridShader, model, view, projection, lightDir);
    terrain.draw();
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

const char* currentModeLabel()
{
    switch (settings.mode) {
    case RenderMode::HeightMap: return settings.wireOverlay ? "Height+Wire" : "HeightMap";
    case RenderMode::Normals:   return settings.wireOverlay ? "Normals+Wire" : "Normals";
    case RenderMode::Shaded:
    default:                    return settings.wireOverlay ? "Shaded+Wire" : "Shaded";
    }
}

// ------------------------------------------------------------------ main
int main()
{
    // --- GLFW init ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);

    window = glfwCreateWindow(SCR_W, SCR_H, "Terrain Renderer | Tessellation", nullptr, nullptr);
    if (!window) { std::cerr << "Failed to create GLFW window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebufferSizeCB);
    glfwSetScrollCallback(window, scrollCB);
    glfwSetCursorPosCallback(window, mouseCB);
    glfwSetKeyCallback(window, keyCB);

    // --- GLAD ---
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n"; return -1;
    }

    // Check tessellation support
    GLint maxPatch;
    glGetIntegerv(GL_MAX_PATCH_VERTICES, &maxPatch);
    std::cout << "GL_MAX_PATCH_VERTICES = " << maxPatch << "\n";
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << "\n";

    // --- Shaders ---
    Shader terrainShader("shaders/terrain.vert",
                         "shaders/terrain.tesc",
                         "shaders/terrain.tese",
                         "shaders/terrain.frag");

    // Wireframe uses same vert/tesc/tese, different frag
    Shader wireShader("shaders/terrain.vert",
                      "shaders/terrain.tesc",
                      "shaders/terrain.tese",
                      "shaders/wire_fine.frag");

    Shader coarseGridShader("shaders/terrain.vert",
                            "shaders/terrain.tesc",
                            "shaders/terrain.tese",
                            "shaders/wire_coarse.frag");

    // --- Terrain mesh ---
    TerrainMesh terrain;
    terrain.build(GRID_N);

    // Tessellation patch size = 4 (quad patches)
    glPatchParameteri(GL_PATCH_VERTICES, 4);

    // --- GL state ---
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.6f, -1.0f, -0.4f));

    // Model: scale terrain to ~40x40 world units, centered at origin
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(20.0f, 1.0f, 20.0f));

    // Print controls
    std::cout << "\n=== Terrain Renderer Controls ===\n";
    std::cout << "  W/A/S/D   : move camera\n";
    std::cout << "  Q/E       : move up/down\n";
    std::cout << "  RMB+drag  : look around\n";
    std::cout << "  Scroll    : zoom FOV\n";
    std::cout << "  1         : shaded mode\n";
    std::cout << "  2         : toggle wireframe overlay\n";
    std::cout << "  3         : height map mode\n";
    std::cout << "  4         : normal map mode\n";
    std::cout << "  +/-       : tessellation max level (current: " << settings.tessMax << ")\n";
    std::cout << "  T         : toggle animated time\n";
    std::cout << "  ESC       : quit\n\n";

    // ------------------------------------------------------------------ loop
    while (!glfwWindowShouldClose(window))
    {
        float now = (float)glfwGetTime();
        deltaTime = now - lastFrame;
        lastFrame = now;
        if (settings.animTime) settings.appTime = now;

        handleMovementInput();

        // -- Projection --
        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glm::mat4 proj = glm::perspective(glm::radians(camera.Fov), (float)fbW / fbH, 0.05f, 500.0f);
        glm::mat4 view = camera.getView();

        // -- Clear --
        glClearColor(0.53f, 0.73f, 0.94f, 1.0f);  // sky blue
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawTerrain(terrain, terrainShader, model, view, proj, lightDir);
        drawWireOverlay(terrain, wireShader, coarseGridShader, model, view, proj, lightDir);

        // -- Window title with stats --
        static double titleTimer = 0;
        titleTimer += deltaTime;
        if (titleTimer > 0.5) {
            titleTimer = 0;
            char buf[256];
            snprintf(buf, sizeof(buf),
                "Terrain | TessMax=%.0f | Mode=%s | %.1f FPS | Cam(%.1f,%.1f,%.1f)",
                settings.tessMax,
                currentModeLabel(),
                1.0f/deltaTime,
                camera.Position.x, camera.Position.y, camera.Position.z);
            glfwSetWindowTitle(window, buf);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
