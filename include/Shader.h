#pragma once
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

class Shader
{
public:
    GLuint ID = 0;

    Shader() = default;

    // Tessellation pipeline: vert + tesc + tese + frag
    Shader(const char* vertPath, const char* tescPath, const char* tesePath, const char* fragPath)
    {
        GLuint vert  = compile(vertPath, GL_VERTEX_SHADER);
        GLuint tesc  = compile(tescPath, GL_TESS_CONTROL_SHADER);
        GLuint tese  = compile(tesePath, GL_TESS_EVALUATION_SHADER);
        GLuint frag  = compile(fragPath, GL_FRAGMENT_SHADER);
        link({vert, tesc, tese, frag});
    }

    // Simple vert + frag pipeline
    Shader(const char* vertPath, const char* fragPath)
    {
        GLuint vert = compile(vertPath, GL_VERTEX_SHADER);
        GLuint frag = compile(fragPath, GL_FRAGMENT_SHADER);
        link({vert, frag});
    }

    void use() const { glUseProgram(ID); }

    void setMat4(const std::string& name, const glm::mat4& m) const {
        glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(m));
    }
    void setVec3(const std::string& name, const glm::vec3& v) const {
        glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(v));
    }
    void setFloat(const std::string& name, float f) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), f);
    }
    void setInt(const std::string& name, int i) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), i);
    }

private:
    GLuint compile(const char* path, GLenum type)
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[Shader] Cannot open: " << path << "\n";
            return 0;
        }
        std::stringstream ss;
        ss << file.rdbuf();
        std::string src = ss.str();
        const char* csrc = src.c_str();

        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &csrc, nullptr);
        glCompileShader(shader);

        GLint ok;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "[Shader] Compile error (" << path << "):\n" << log << "\n";
        }
        return shader;
    }

    void link(std::initializer_list<GLuint> shaders)
    {
        ID = glCreateProgram();
        for (GLuint s : shaders) if (s) glAttachShader(ID, s);
        glLinkProgram(ID);

        GLint ok;
        glGetProgramiv(ID, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetProgramInfoLog(ID, sizeof(log), nullptr, log);
            std::cerr << "[Shader] Link error:\n" << log << "\n";
        }
        for (GLuint s : shaders) if (s) glDeleteShader(s);
    }
};
