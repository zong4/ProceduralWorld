add_rules("mode.debug", "mode.release")
set_defaultmode("debug")

set_project("ProceduralWorld")
set_version("1.0.0")
set_languages("c++17")

-- Add required packages
add_requires("glfw", "glad", "glm")
add_requires("imgui", {configs = {glfw = true, opengl3 = true}})

target("ProceduralWorld")
    set_kind("binary")
    add_files("src/*.cpp")
    add_includedirs("include")
    add_includedirs("third_party/stb")
    add_packages("glfw", "glad", "glm", "imgui")

    after_build(function(target)
        local shader_dst = path.join(target:targetdir(), "shaders")

        -- Delete the old shader directory if it exists
        if os.isdir(shader_dst) then
            os.rm(shader_dst)
        end

        -- Copy the shaders directory to the target output directory
        os.cp("shaders", shader_dst)

        local asset_dst = path.join(target:targetdir(), "assets")
        if os.isdir(asset_dst) then
            os.rm(asset_dst)
        end
        os.cp("assets", asset_dst)
    end)

    if is_plat("windows") then
        add_links("opengl32")
    elseif is_plat("macosx") then
        add_frameworks("OpenGL")
        add_ldflags("-framework CoreFoundation")
    elseif is_plat("linux") then
        add_links("GL", "dl", "pthread")
    end
