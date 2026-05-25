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

extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace {

enum class ControlPanel {
    View,
    Terrain,
    Materials,
    Water,
    Sky,
    Clouds,
};

struct AppState {
    bool dragging = false;
    bool mouse_active = false;
    double last_x = 0.0;
    double last_y = 0.0;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float zoom = 1.0f;
    float render_scale = 0.60f;
    float terrain_height = 0.4f;
    float sea_level = 0.18f;
    bool enable_erosion = true;
    float erosion_strength = 0.82f;
    bool enable_materials = true;
    float material_detail = 1.0f;
    bool enable_vegetation = true;
    float vegetation_density = 1.15f;
    float tree_density = 0.85f;
    float tree_height = 0.65f;
    float tree_scale = 72.0f;
    bool enable_water = true;
    float water_reflection_strength = 0.72f;
    float water_refraction_strength = 0.58f;
    float water_wave_strength = 0.70f;
    bool enable_clouds = true;
    float cloud_coverage = 0.29475675f;
    float cloud_fuzzy = 0.0335f;
    float cloud_absorb = 30.034f;
    float light_strength = 1.0f;
    float sun_azimuth = 42.0f;
    float sun_elevation = 28.0f;
    float sky_exposure = 1.0f;
    float planet_speed = 1.0f;
    float cloud_speed = 1.0f;
    bool paused = false;
    bool vsync = true;
    ControlPanel active_panel = ControlPanel::View;
};

struct ShaderUniforms {
    GLint resolution = -1;
    GLint time = -1;
    GLint mouse = -1;
    GLint render_scale = -1;
    GLint zoom = -1;
    GLint terrain_height = -1;
    GLint sea_level = -1;
    GLint enable_erosion = -1;
    GLint erosion_strength = -1;
    GLint enable_materials = -1;
    GLint material_detail = -1;
    GLint enable_vegetation = -1;
    GLint vegetation_density = -1;
    GLint tree_density = -1;
    GLint tree_height = -1;
    GLint tree_scale = -1;
    GLint enable_water = -1;
    GLint water_reflection_strength = -1;
    GLint water_refraction_strength = -1;
    GLint water_wave_strength = -1;
    GLint enable_clouds = -1;
    GLint cloud_coverage = -1;
    GLint cloud_fuzzy = -1;
    GLint cloud_absorb = -1;
    GLint light_strength = -1;
    GLint sun_azimuth = -1;
    GLint sun_elevation = -1;
    GLint sky_exposure = -1;
    GLint planet_speed = -1;
    GLint cloud_speed = -1;
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

std::string parse_shader_include(const std::string& line)
{
    const size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line.compare(first, 8, "#include") != 0) {
        return {};
    }

    const size_t after_directive = first + 8;
    if (after_directive < line.size() && line[after_directive] != ' ' && line[after_directive] != '\t') {
        return {};
    }

    const size_t open = line.find_first_of("\"<", after_directive);
    if (open == std::string::npos) {
        return {};
    }

    const char close_char = line[open] == '"' ? '"' : '>';
    const size_t close = line.find(close_char, open + 1);
    if (close == std::string::npos) {
        return {};
    }

    return line.substr(open + 1, close - open - 1);
}

std::filesystem::path resolve_shader_include(
    const std::filesystem::path& include_path,
    const std::filesystem::path& current_dir)
{
    std::vector<std::filesystem::path> candidates;
    if (include_path.is_absolute()) {
        candidates.push_back(include_path);
    } else {
        candidates.push_back(current_dir / include_path);
        candidates.push_back(std::filesystem::path(PCG_PROJECT_ROOT) / include_path);
        candidates.push_back(std::filesystem::current_path() / include_path);
    }

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            return std::filesystem::canonical(candidate, ec);
        }
    }

    throw std::runtime_error("Shader include not found: " + include_path.string());
}

std::string expand_shader_includes(
    const std::filesystem::path& path,
    std::vector<std::filesystem::path>& include_stack)
{
    std::error_code ec;
    auto canonical_path = std::filesystem::canonical(path, ec);
    if (ec) {
        throw std::runtime_error("Shader file does not exist: " + path.string());
    }

    if (std::find(include_stack.begin(), include_stack.end(), canonical_path) != include_stack.end()) {
        throw std::runtime_error("Cyclic shader include detected at: " + canonical_path.string());
    }

    include_stack.push_back(canonical_path);

    std::istringstream input(read_text_file(canonical_path));
    std::ostringstream output;
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        const std::string include_name = parse_shader_include(line);
        if (include_name.empty()) {
            output << line << '\n';
            continue;
        }

        try {
            const auto include_file = resolve_shader_include(include_name, canonical_path.parent_path());
            output << expand_shader_includes(include_file, include_stack);
            output << '\n';
        } catch (const std::exception& e) {
            throw std::runtime_error(
                canonical_path.string() + ":" + std::to_string(line_number) + ": " + e.what());
        }
    }

    include_stack.pop_back();
    return output.str();
}

std::string load_shader_source(const std::filesystem::path& path)
{
    std::vector<std::filesystem::path> include_stack;
    return expand_shader_includes(path, include_stack);
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
        std::filesystem::current_path() / "shaders" / "planet" / "planet_shader.glsl",
        std::filesystem::path(PCG_PROJECT_ROOT) / "shaders" / "planet" / "planet_shader.glsl",
        exe_dir / "shaders" / "planet" / "planet_shader.glsl",
        exe_dir / ".." / "shaders" / "planet" / "planet_shader.glsl",
        exe_dir / ".." / ".." / "shaders" / "planet" / "planet_shader.glsl",
    };

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec)) {
            return std::filesystem::canonical(candidate, ec);
        }
    }

    throw std::runtime_error("Could not find shader file: shaders/planet/planet_shader.glsl");
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
uniform float u_render_scale;
uniform float iZoom;
uniform float u_terrain_height;
uniform float u_sea_level;
uniform float u_enable_erosion;
uniform float u_erosion_strength;
uniform float u_enable_materials;
uniform float u_material_detail;
uniform float u_enable_vegetation;
uniform float u_vegetation_density;
uniform float u_tree_density;
uniform float u_tree_height;
uniform float u_tree_scale;
uniform float u_enable_water;
uniform float u_water_reflection_strength;
uniform float u_water_refraction_strength;
uniform float u_water_wave_strength;
uniform float u_enable_clouds;
uniform float u_cloud_coverage;
uniform float u_cloud_fuzzy;
uniform float u_cloud_absorb;
uniform float u_light_strength;
uniform float u_sun_azimuth;
uniform float u_sun_elevation;
uniform float u_sky_exposure;
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

    const std::string fragment_source = build_fragment_shader(load_shader_source(shader_path));
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

void ensure_render_target(
    GLuint& framebuffer,
    GLuint& color_texture,
    int& current_width,
    int& current_height,
    int width,
    int height)
{
    if (framebuffer != 0 && color_texture != 0 && current_width == width && current_height == height) {
        return;
    }

    if (framebuffer == 0) {
        glGenFramebuffers(1, &framebuffer);
    }
    if (color_texture == 0) {
        glGenTextures(1, &color_texture);
    }

    current_width = width;
    current_height = height;

    glBindTexture(GL_TEXTURE_2D, color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Render-scale framebuffer is incomplete");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

ShaderUniforms query_uniforms(GLuint program)
{
    ShaderUniforms u;
    u.resolution = glGetUniformLocation(program, "iResolution");
    u.time = glGetUniformLocation(program, "iTime");
    u.mouse = glGetUniformLocation(program, "iMouse");
    u.render_scale = glGetUniformLocation(program, "u_render_scale");
    u.zoom = glGetUniformLocation(program, "iZoom");
    u.terrain_height = glGetUniformLocation(program, "u_terrain_height");
    u.sea_level = glGetUniformLocation(program, "u_sea_level");
    u.enable_erosion = glGetUniformLocation(program, "u_enable_erosion");
    u.erosion_strength = glGetUniformLocation(program, "u_erosion_strength");
    u.enable_materials = glGetUniformLocation(program, "u_enable_materials");
    u.material_detail = glGetUniformLocation(program, "u_material_detail");
    u.enable_vegetation = glGetUniformLocation(program, "u_enable_vegetation");
    u.vegetation_density = glGetUniformLocation(program, "u_vegetation_density");
    u.tree_density = glGetUniformLocation(program, "u_tree_density");
    u.tree_height = glGetUniformLocation(program, "u_tree_height");
    u.tree_scale = glGetUniformLocation(program, "u_tree_scale");
    u.enable_water = glGetUniformLocation(program, "u_enable_water");
    u.water_reflection_strength = glGetUniformLocation(program, "u_water_reflection_strength");
    u.water_refraction_strength = glGetUniformLocation(program, "u_water_refraction_strength");
    u.water_wave_strength = glGetUniformLocation(program, "u_water_wave_strength");
    u.enable_clouds = glGetUniformLocation(program, "u_enable_clouds");
    u.cloud_coverage = glGetUniformLocation(program, "u_cloud_coverage");
    u.cloud_fuzzy = glGetUniformLocation(program, "u_cloud_fuzzy");
    u.cloud_absorb = glGetUniformLocation(program, "u_cloud_absorb");
    u.light_strength = glGetUniformLocation(program, "u_light_strength");
    u.sun_azimuth = glGetUniformLocation(program, "u_sun_azimuth");
    u.sun_elevation = glGetUniformLocation(program, "u_sun_elevation");
    u.sky_exposure = glGetUniformLocation(program, "u_sky_exposure");
    u.planet_speed = glGetUniformLocation(program, "u_planet_speed");
    u.cloud_speed = glGetUniformLocation(program, "u_cloud_speed");
    return u;
}

void upload_uniforms(
    const ShaderUniforms& u,
    const AppState& state,
    int render_width,
    int render_height,
    float render_scale,
    float time)
{
    glUniform3f(u.resolution, static_cast<float>(render_width), static_cast<float>(render_height), 1.0f);
    glUniform1f(u.time, time);
    glUniform1f(u.render_scale, render_scale);
    glUniform1f(u.zoom, state.zoom);
    glUniform1f(u.terrain_height, state.terrain_height);
    glUniform1f(u.sea_level, state.sea_level);
    glUniform1f(u.enable_erosion, state.enable_erosion ? 1.0f : 0.0f);
    glUniform1f(u.erosion_strength, state.erosion_strength);
    glUniform1f(u.enable_materials, state.enable_materials ? 1.0f : 0.0f);
    glUniform1f(u.material_detail, state.material_detail);
    glUniform1f(u.enable_vegetation, state.enable_vegetation ? 1.0f : 0.0f);
    glUniform1f(u.vegetation_density, state.vegetation_density);
    glUniform1f(u.tree_density, state.tree_density);
    glUniform1f(u.tree_height, state.tree_height);
    glUniform1f(u.tree_scale, state.tree_scale);
    glUniform1f(u.enable_water, state.enable_water ? 1.0f : 0.0f);
    glUniform1f(u.water_reflection_strength, state.water_reflection_strength);
    glUniform1f(u.water_refraction_strength, state.water_refraction_strength);
    glUniform1f(u.water_wave_strength, state.water_wave_strength);
    glUniform1f(u.enable_clouds, state.enable_clouds ? 1.0f : 0.0f);
    glUniform1f(u.cloud_coverage, state.cloud_coverage);
    glUniform1f(u.cloud_fuzzy, state.cloud_fuzzy);
    glUniform1f(u.cloud_absorb, state.cloud_absorb);
    glUniform1f(u.light_strength, state.light_strength);
    glUniform1f(u.sun_azimuth, state.sun_azimuth);
    glUniform1f(u.sun_elevation, state.sun_elevation);
    glUniform1f(u.sky_exposure, state.sky_exposure);
    glUniform1f(u.planet_speed, state.planet_speed);
    glUniform1f(u.cloud_speed, state.cloud_speed);
    glUniform4f(
        u.mouse,
        state.yaw,
        state.pitch,
        state.mouse_active ? 1.0f : 0.0f,
        state.dragging ? 1.0f : 0.0f);
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

bool panel_button(AppState& state, ControlPanel panel, const char* label)
{
    const bool active = state.active_panel == panel;
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }

    const bool clicked = ImGui::Button(label);
    if (clicked) {
        state.active_panel = panel;
    }

    if (active) {
        ImGui::PopStyleColor(2);
    }
    return clicked;
}

void draw_control_panel(AppState& state)
{
    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Raymarch Controls");

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS %.1f  Frame %.2f ms", io.Framerate, 1000.0f / std::max(io.Framerate, 0.001f));
    ImGui::Separator();

    panel_button(state, ControlPanel::View, "View");
    ImGui::SameLine();
    panel_button(state, ControlPanel::Terrain, "Terrain");
    ImGui::SameLine();
    panel_button(state, ControlPanel::Materials, "Materials");
    panel_button(state, ControlPanel::Water, "Water");
    ImGui::SameLine();
    panel_button(state, ControlPanel::Sky, "Sun / Sky");
    ImGui::SameLine();
    panel_button(state, ControlPanel::Clouds, "Clouds");

    ImGui::Separator();

    switch (state.active_panel) {
    case ControlPanel::View:
        ImGui::SliderFloat("Render scale", &state.render_scale, 0.35f, 1.0f, "%.2f");
        ImGui::SliderFloat("Zoom", &state.zoom, 0.25f, 2.5f, "%.2f");
        ImGui::SliderFloat("Planet speed", &state.planet_speed, -3.0f, 3.0f, "%.2f");
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
        break;
    case ControlPanel::Terrain:
        ImGui::SliderFloat("Terrain height", &state.terrain_height, 0.1f, 0.8f, "%.3f");
        ImGui::SliderFloat("Sea level", &state.sea_level, 0.05f, 0.55f, "%.3f");
        ImGui::Checkbox("Enable erosion", &state.enable_erosion);
        ImGui::BeginDisabled(!state.enable_erosion);
        ImGui::SliderFloat("Erosion strength", &state.erosion_strength, 0.0f, 1.4f, "%.2f");
        ImGui::EndDisabled();
        break;
    case ControlPanel::Materials:
        ImGui::Checkbox("Enable material detail", &state.enable_materials);
        ImGui::BeginDisabled(!state.enable_materials);
        ImGui::SliderFloat("Material detail", &state.material_detail, 0.0f, 1.5f, "%.2f");
        ImGui::EndDisabled();
        ImGui::Checkbox("Enable vegetation", &state.enable_vegetation);
        ImGui::BeginDisabled(!state.enable_vegetation);
        ImGui::SliderFloat("Vegetation density", &state.vegetation_density, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Tree density", &state.tree_density, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Tree height", &state.tree_height, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Tree scale", &state.tree_scale, 24.0f, 160.0f, "%.0f");
        ImGui::EndDisabled();
        break;
    case ControlPanel::Water:
        ImGui::Checkbox("Enable water", &state.enable_water);
        ImGui::BeginDisabled(!state.enable_water);
        ImGui::SliderFloat("Water reflection", &state.water_reflection_strength, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Water refraction", &state.water_refraction_strength, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Water waves", &state.water_wave_strength, 0.0f, 1.5f, "%.2f");
        ImGui::EndDisabled();
        break;
    case ControlPanel::Sky:
        ImGui::SliderFloat("Light", &state.light_strength, 0.0f, 2.5f, "%.2f");
        ImGui::SliderFloat("Sun azimuth", &state.sun_azimuth, -180.0f, 180.0f, "%.1f");
        ImGui::SliderFloat("Sun elevation", &state.sun_elevation, -8.0f, 82.0f, "%.1f");
        ImGui::SliderFloat("Sky exposure", &state.sky_exposure, 0.2f, 2.2f, "%.2f");
        break;
    case ControlPanel::Clouds:
        ImGui::Checkbox("Enable clouds", &state.enable_clouds);
        ImGui::BeginDisabled(!state.enable_clouds);
        ImGui::SliderFloat("Cloud coverage", &state.cloud_coverage, 0.0f, 0.9f, "%.3f");
        ImGui::SliderFloat("Cloud fuzzy", &state.cloud_fuzzy, 0.001f, 0.25f, "%.3f");
        ImGui::SliderFloat("Cloud absorb", &state.cloud_absorb, 1.0f, 80.0f, "%.2f");
        ImGui::SliderFloat("Cloud speed", &state.cloud_speed, -3.0f, 3.0f, "%.2f");
        ImGui::EndDisabled();
        break;
    }

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
        std::cout << "GPU vendor: " << glGetString(GL_VENDOR) << '\n';
        std::cout << "GPU renderer: " << glGetString(GL_RENDERER) << '\n';

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

        const ShaderUniforms uniforms = query_uniforms(program);

        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        GLuint render_fbo = 0;
        GLuint render_color = 0;
        int render_target_width = 0;
        int render_target_height = 0;

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
            const float render_scale = std::clamp(state.render_scale, 0.35f, 1.0f);
            const int render_width = std::max(1, static_cast<int>(static_cast<float>(width) * render_scale));
            const int render_height = std::max(1, static_cast<int>(static_cast<float>(height) * render_scale));
            ensure_render_target(
                render_fbo,
                render_color,
                render_target_width,
                render_target_height,
                render_width,
                render_height);
            glBindFramebuffer(GL_FRAMEBUFFER, render_fbo);
            glViewport(0, 0, render_width, render_height);

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
            upload_uniforms(uniforms, state, render_width, render_height, render_scale, paused_time);

            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, render_fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(
                0,
                0,
                render_width,
                render_height,
                0,
                0,
                width,
                height,
                GL_COLOR_BUFFER_BIT,
                GL_LINEAR);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }

        glDeleteVertexArrays(1, &vao);
        if (render_color != 0) {
            glDeleteTextures(1, &render_color);
        }
        if (render_fbo != 0) {
            glDeleteFramebuffers(1, &render_fbo);
        }
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
