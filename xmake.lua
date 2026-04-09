add_rules("mode.debug", "mode.release")
set_defaultmode("debug")

set_project("ProceduralWorld")
set_version("1.0.0")
set_languages("c++17")

-- Add required packages
add_requires("glfw", "glad", "glm")

target("ProceduralWorld")
    set_kind("binary")
    add_files("src/*.cpp")
    add_includedirs("include")
    add_packages("glfw", "glad", "glm")
    
    -- Copy shaders to build directory
    after_build(function(target)
        os.cp("shaders", path.join(target:targetdir(), "shaders"))
    end)
    
    if is_plat("windows") then
        add_links("opengl32")
    elseif is_plat("macosx") then
        add_frameworks("OpenGL")
        add_ldflags("-framework CoreFoundation")
    elseif is_plat("linux") then
        add_links("GL", "dl", "pthread")
    end
