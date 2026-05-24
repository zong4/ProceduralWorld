#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef PCG_PROJECT_ROOT
#define PCG_PROJECT_ROOT "."
#endif

namespace {

struct AppState {
    bool dragging = false;
    bool mouse_active = false;
    double last_x = 0.0;
    double last_y = 0.0;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float zoom = 1.0f;
    float terrain_height = 0.4f;
    float cloud_coverage = 0.29475675f;
    float cloud_fuzzy = 0.0335f;
    float cloud_absorb = 30.034f;
    float light_strength = 1.0f;
    float planet_speed = 1.0f;
    float cloud_speed = 1.0f;
    bool paused = false;
    bool vsync = true;
};

void glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW error " << error << ": " << description << '\n';
}

std::string read_text_file(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open shader file: " + path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::filesystem::path executable_dir(const char* argv0)
{
    std::error_code ec;
    auto p = std::filesystem::absolute(argv0, ec);
    if (ec) {
        return std::filesystem::current_path();
    }
    return p.parent_path();
}

std::filesystem::path find_shader_path(int argc, char** argv)
{
    if (argc > 1) {
        if (std::string(argv[1]) == "--check") {
            if (argc > 2) {
                std::filesystem::path explicit_path = argv[2];
                if (std::filesystem::exists(explicit_path)) {
                    return explicit_path;
                }
                throw std::runtime_error("Shader path does not exist: " + explicit_path.string());
            }
        } else {
            std::filesystem::path explicit_path = argv[1];
            if (std::filesystem::exists(explicit_path)) {
                return explicit_path;
            }
            throw std::runtime_error("Shader path does not exist: " + explicit_path.string());
        }
    }

    const auto exe_dir = executable_dir(argv[0]);
    const std::vector<std::filesystem::path> candidates = {
        std::filesystem::current_path() / "aaa",
        std::filesystem::path(PCG_PROJECT_ROOT) / "aaa",
        exe_dir / "aaa",
        exe_dir / ".." / "aaa",
        exe_dir / ".." / ".." / "aaa",
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            return std::filesystem::canonical(candidate, ec);
        }
    }

    throw std::runtime_error("Could not find shader file named aaa");
}

bool has_check_flag(int argc, char** argv)
{
    return argc > 1 && std::string(argv[1]) == "--check";
}

std::string build_fragment_shader(const std::string& shadertoy_source)
{
    return std::string(R"GLSL(#version 330 core
uniform vec3 iResolution;
uniform float iTime;
uniform vec4 iMouse;
uniform float iZoom;
uniform float u_terrain_height;
uniform float u_cloud_coverage;
uniform float u_cloud_fuzzy;
uniform float u_cloud_absorb;
uniform float u_light_strength;
uniform float u_planet_speed;
uniform float u_cloud_speed;

#define _in(T) const in T
#define _inout(T) inout T
#define _out(T) out T
#define _begin(type) type (
#define _end )
#define _mutable(T) T
#define _constant(T) const T
#define mul(a, b) ((a) * (b))
)GLSL") + shadertoy_source + R"GLSL(

out vec4 FragColor;

void main()
{
    mainImage(FragColor, gl_FragCoord.xy);
}
)GLSL";
}

GLuint compile_shader(GLenum type, const std::string& source, const char* label)
{
    const GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) {
        return shader;
    }

    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    std::string log(static_cast<size_t>(std::max(log_length, 1)), '\0');
    glGetShaderInfoLog(shader, log_length, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error(std::string(label) + " compile failed:\n" + log);
}

GLuint link_program(GLuint vertex_shader, GLuint fragment_shader)
{
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok == GL_TRUE) {
        return program;
    }

    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    std::string log(static_cast<size_t>(std::max(log_length, 1)), '\0');
    glGetProgramInfoLog(program, log_length, nullptr, log.data());
    glDeleteProgram(program);
    throw std::runtime_error("Program link failed:\n" + log);
}

GLuint create_program(const std::filesystem::path& shader_path)
{
    const std::string vertex_source = R"GLSL(#version 330 core
const vec2 kPositions[3] = vec2[](
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

void main()
{
    gl_Position = vec4(kPositions[gl_VertexID], 0.0, 1.0);
}
)GLSL";

    const std::string fragment_source = build_fragment_shader(read_text_file(shader_path));
    const GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_source, "Vertex shader");
    const GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_source, "Fragment shader");

    GLuint program = 0;
    try {
        program = link_program(vs, fs);
    } catch (...) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        throw;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

void cursor_position_callback(GLFWwindow* window, double x, double y)
{
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        state->dragging = false;
        state->last_x = x;
        state->last_y = y;
        return;
    }

    if (state->dragging) {
        const double dx = x - state->last_x;
        const double dy = y - state->last_y;
        state->yaw += static_cast<float>(dx * 0.25);
        state->pitch += static_cast<float>(dy * 0.25);
        state->pitch = std::clamp(state->pitch, -85.0f, 85.0f);
    }

    state->last_x = x;
    state->last_y = y;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int)
{
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;
    }

    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        state->dragging = false;
        return;
    }

    if (action == GLFW_PRESS) {
        state->dragging = true;
        state->mouse_active = true;
        glfwGetCursorPos(window, &state->last_x, &state->last_y);
    } else if (action == GLFW_RELEASE) {
        state->dragging = false;
    }
}

void scroll_callback(GLFWwindow* window, double, double yoffset)
{
    auto* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) {
        return;
    }

    state->zoom *= static_cast<float>(std::pow(0.88, yoffset));
    state->zoom = std::clamp(state->zoom, 0.25f, 2.5f);
}

void draw_control_panel(AppState& state)
{
    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(330.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Raymarch Controls");

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS %.1f  Frame %.2f ms", io.Framerate, 1000.0f / std::max(io.Framerate, 0.001f));
    ImGui::Separator();

    ImGui::Checkbox("Pause time", &state.paused);
    if (ImGui::Checkbox("VSync", &state.vsync)) {
        glfwSwapInterval(state.vsync ? 1 : 0);
    }

    if (ImGui::Button("Reset view")) {
        state.zoom = 1.0f;
        state.yaw = 0.0f;
        state.pitch = 0.0f;
        state.mouse_active = false;
    }

    ImGui::Separator();
    ImGui::SliderFloat("Zoom", &state.zoom, 0.25f, 2.5f, "%.2f");
    ImGui::SliderFloat("Terrain height", &state.terrain_height, 0.1f, 0.8f, "%.3f");
    ImGui::SliderFloat("Light", &state.light_strength, 0.0f, 2.5f, "%.2f");
    ImGui::SliderFloat("Planet speed", &state.planet_speed, -3.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Cloud speed", &state.cloud_speed, -3.0f, 3.0f, "%.2f");

    ImGui::Separator();
    ImGui::SliderFloat("Cloud coverage", &state.cloud_coverage, 0.0f, 0.9f, "%.3f");
    ImGui::SliderFloat("Cloud fuzzy", &state.cloud_fuzzy, 0.001f, 0.25f, "%.3f");
    ImGui::SliderFloat("Cloud absorb", &state.cloud_absorb, 1.0f, 80.0f, "%.2f");

    ImGui::End();
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const auto shader_path = find_shader_path(argc, argv);
        const bool check_only = has_check_flag(argc, argv);
        std::cout << "Loading shader: " << shader_path.string() << '\n';

        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit()) {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        if (check_only) {
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        }

        GLFWwindow* window = glfwCreateWindow(1280, 720, "Raymarch SDF Planet", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
            glfwDestroyWindow(window);
            glfwTerminate();
            throw std::runtime_error("Failed to load OpenGL functions");
        }

        std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';

        AppState state;
        glfwSetWindowUserPointer(window, &state);
        glfwSetCursorPosCallback(window, cursor_position_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetScrollCallback(window, scroll_callback);

        const GLuint program = create_program(shader_path);
        if (check_only) {
            std::cout << "Shader check ok\n";
            glDeleteProgram(program);
            glfwDestroyWindow(window);
            glfwTerminate();
            return 0;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        const GLint resolution_loc = glGetUniformLocation(program, "iResolution");
        const GLint time_loc = glGetUniformLocation(program, "iTime");
        const GLint mouse_loc = glGetUniformLocation(program, "iMouse");
        const GLint zoom_loc = glGetUniformLocation(program, "iZoom");
        const GLint terrain_height_loc = glGetUniformLocation(program, "u_terrain_height");
        const GLint cloud_coverage_loc = glGetUniformLocation(program, "u_cloud_coverage");
        const GLint cloud_fuzzy_loc = glGetUniformLocation(program, "u_cloud_fuzzy");
        const GLint cloud_absorb_loc = glGetUniformLocation(program, "u_cloud_absorb");
        const GLint light_strength_loc = glGetUniformLocation(program, "u_light_strength");
        const GLint planet_speed_loc = glGetUniformLocation(program, "u_planet_speed");
        const GLint cloud_speed_loc = glGetUniformLocation(program, "u_cloud_speed");

        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        const auto start_time = std::chrono::steady_clock::now();
        float paused_time = 0.0f;
        bool was_paused = false;
        while (!glfwWindowShouldClose(window)) {
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }

            glfwPollEvents();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            draw_control_panel(state);
            ImGui::Render();

            int width = 0;
            int height = 0;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);

            const auto now = std::chrono::steady_clock::now();
            const float elapsed = std::chrono::duration<float>(now - start_time).count();
            if (!state.paused) {
                paused_time = elapsed;
            } else if (!was_paused) {
                paused_time = elapsed;
            }
            was_paused = state.paused;

            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(program);
            glUniform3f(resolution_loc, static_cast<float>(width), static_cast<float>(height), 1.0f);
            glUniform1f(time_loc, paused_time);
            glUniform1f(zoom_loc, state.zoom);
            glUniform1f(terrain_height_loc, state.terrain_height);
            glUniform1f(cloud_coverage_loc, state.cloud_coverage);
            glUniform1f(cloud_fuzzy_loc, state.cloud_fuzzy);
            glUniform1f(cloud_absorb_loc, state.cloud_absorb);
            glUniform1f(light_strength_loc, state.light_strength);
            glUniform1f(planet_speed_loc, state.planet_speed);
            glUniform1f(cloud_speed_loc, state.cloud_speed);
            glUniform4f(
                mouse_loc,
                state.yaw,
                state.pitch,
                state.mouse_active ? 1.0f : 0.0f,
                state.dragging ? 1.0f : 0.0f);

            glDrawArrays(GL_TRIANGLES, 0, 3);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }

        glDeleteVertexArrays(1, &vao);
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glDeleteProgram(program);
        glfwDestroyWindow(window);
        glfwTerminate();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
