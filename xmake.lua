set_project("pcg-raymarch-sdf")
set_version("0.1.0")
set_languages("cxx17")

add_rules("mode.debug", "mode.release")

add_requires("glfw", "glad")
add_requires("imgui", {configs = {glfw = true, opengl3 = true}})

target("pcg_raymarch")
    set_kind("binary")
    add_files("src/main.cpp")
    add_packages("glfw", "glad", "imgui")

    local project_root = os.projectdir():gsub("\\", "\\\\")
    add_defines('PCG_PROJECT_ROOT="' .. project_root .. '"')
