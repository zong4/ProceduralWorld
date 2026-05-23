#pragma once

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

// OpenGL shader 编译/链接小工具。
// 特点：
// - 支持 VS/FS 和 VS/TCS/TES/FS 两套构造方式；
// - 在编译前递归展开本地 #include "xxx.glsl"；
// - 提供常用 uniform setter，减少渲染代码中的 OpenGL 样板。
class ShaderProgram
{
public:
    GLuint programId = 0;

    ShaderProgram() = default;

    ShaderProgram(const char* vertexPath,
                  const char* tessControlPath,
                  const char* tessEvaluationPath,
                  const char* fragmentPath)
    {
        const GLuint vertexShader = compileShader(vertexPath, GL_VERTEX_SHADER);
        const GLuint tessControlShader = compileShader(tessControlPath, GL_TESS_CONTROL_SHADER);
        const GLuint tessEvaluationShader = compileShader(tessEvaluationPath, GL_TESS_EVALUATION_SHADER);
        const GLuint fragmentShader = compileShader(fragmentPath, GL_FRAGMENT_SHADER);
        linkProgram({vertexShader, tessControlShader, tessEvaluationShader, fragmentShader});
    }

    ShaderProgram(const char* vertexPath, const char* fragmentPath)
    {
        const GLuint vertexShader = compileShader(vertexPath, GL_VERTEX_SHADER);
        const GLuint fragmentShader = compileShader(fragmentPath, GL_FRAGMENT_SHADER);
        linkProgram({vertexShader, fragmentShader});
    }

    void use() const
    {
        glUseProgram(programId);
    }

    void setMat4(const std::string& uniformName, const glm::mat4& value) const
    {
        glUniformMatrix4fv(glGetUniformLocation(programId, uniformName.c_str()), 1, GL_FALSE, glm::value_ptr(value));
    }

    void setVec2(const std::string& uniformName, const glm::vec2& value) const
    {
        glUniform2fv(glGetUniformLocation(programId, uniformName.c_str()), 1, glm::value_ptr(value));
    }

    void setVec3(const std::string& uniformName, const glm::vec3& value) const
    {
        glUniform3fv(glGetUniformLocation(programId, uniformName.c_str()), 1, glm::value_ptr(value));
    }

    void setVec4(const std::string& uniformName, const glm::vec4& value) const
    {
        glUniform4fv(glGetUniformLocation(programId, uniformName.c_str()), 1, glm::value_ptr(value));
    }

    void setFloat(const std::string& uniformName, float value) const
    {
        glUniform1f(glGetUniformLocation(programId, uniformName.c_str()), value);
    }

    void setInt(const std::string& uniformName, int value) const
    {
        glUniform1i(glGetUniformLocation(programId, uniformName.c_str()), value);
    }

private:
    // 递归展开 GLSL include。includeStack 用来检测循环包含，避免无限递归。
    static std::string expandIncludes(const std::filesystem::path& filePath,
                                      std::vector<std::filesystem::path>& includeStack)
    {
        const std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(filePath);
        for (const std::filesystem::path& activePath : includeStack) {
            if (activePath == normalizedPath) {
                std::cerr << "[ShaderProgram] Include cycle detected at: " << normalizedPath.string() << "\n";
                return {};
            }
        }

        std::ifstream sourceFile(normalizedPath);
        if (!sourceFile.is_open()) {
            std::cerr << "[ShaderProgram] Cannot open: " << normalizedPath.string() << "\n";
            return {};
        }

        includeStack.push_back(normalizedPath);

        std::ostringstream expandedSource;
        std::string line;
        while (std::getline(sourceFile, line)) {
            const std::size_t includePos = line.find("#include");
            if (includePos == std::string::npos) {
                expandedSource << line << '\n';
                continue;
            }

            const std::size_t firstQuote = line.find('"', includePos);
            const std::size_t lastQuote = line.find_last_of('"');
            if (firstQuote == std::string::npos || lastQuote == std::string::npos || lastQuote <= firstQuote) {
                std::cerr << "[ShaderProgram] Malformed include in: " << normalizedPath.string() << "\n";
                includeStack.pop_back();
                return {};
            }

            const std::string includeName = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
            const std::filesystem::path includePath = normalizedPath.parent_path() / includeName;
            expandedSource << expandIncludes(includePath, includeStack);
        }

        includeStack.pop_back();
        return expandedSource.str();
    }

    // 读取、展开、编译单个 shader stage。编译错误直接打印到 stderr。
    static GLuint compileShader(const char* filePath, GLenum shaderType)
    {
        std::vector<std::filesystem::path> includeStack;
        const std::string sourceText = expandIncludes(filePath, includeStack);
        if (sourceText.empty()) {
            return 0;
        }
        const char* sourcePtr = sourceText.c_str();

        const GLuint shader = glCreateShader(shaderType);
        glShaderSource(shader, 1, &sourcePtr, nullptr);
        glCompileShader(shader);

        GLint compileSucceeded = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compileSucceeded);
        if (!compileSucceeded) {
            char infoLog[2048];
            glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
            std::cerr << "[ShaderProgram] Compile error (" << filePath << "):\n" << infoLog << "\n";
        }

        return shader;
    }

    // 将已编译的 shader stage 链接成 program；链接后删除中间 shader 对象。
    void linkProgram(std::initializer_list<GLuint> shaders)
    {
        programId = glCreateProgram();
        for (const GLuint shader : shaders) {
            if (shader != 0) {
                glAttachShader(programId, shader);
            }
        }

        glLinkProgram(programId);

        GLint linkSucceeded = 0;
        glGetProgramiv(programId, GL_LINK_STATUS, &linkSucceeded);
        if (!linkSucceeded) {
            char infoLog[2048];
            glGetProgramInfoLog(programId, sizeof(infoLog), nullptr, infoLog);
            std::cerr << "[ShaderProgram] Link error:\n" << infoLog << "\n";
        }

        for (const GLuint shader : shaders) {
            if (shader != 0) {
                glDeleteShader(shader);
            }
        }
    }
};
