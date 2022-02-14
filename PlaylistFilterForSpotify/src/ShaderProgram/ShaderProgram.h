#pragma once

// #include <ShaderInstance/ShaderInstance.h>
#include <glad/glad.h>
#include <glm/ext.hpp>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#define VERTEX_SHADER_BIT 1U
#define TESS_CONTROL_BIT 2U
#define TESS_EVAL_BIT 4U
#define GEOMETRY_SHADER_BIT 8U
#define FRAGMENT_SHADER_BIT 16U
#define COMPUTE_SHADER_BIT 32U

class ShaderInstance;

using definePair = std::pair<std::string, std::string>;

class ShaderProgram
{
  public:
    ShaderProgram() = default;
    ShaderProgram(
        GLuint shader_mask, std::initializer_list<std::string> l,
        std::initializer_list<definePair> defines = {});
    ShaderProgram& operator=(ShaderProgram&& other) noexcept;
    // Delete until I actually need them, dont want anything implicit
    ShaderProgram(const ShaderProgram&) = delete;            // copy constr
    ShaderProgram& operator=(const ShaderProgram&) = delete; // copy assign
    ShaderProgram(ShaderProgram&&) = delete;                 // move constr
    ~ShaderProgram();

    void GenerateShaderProgram(GLuint shader_mask, std::vector<std::string> ShaderNames);

    void UseProgram();
    GLint getUniformLocation(std::string s);
    GLuint getProgramID();

    void setInt(const GLchar* name, int value);
    void setInt2(const GLchar* name, glm::ivec2 value);
    void setUint(const GLchar* name, GLuint value);
    void setFloat(const GLchar* name, float value);
    void setVec2(const GLchar* name, glm::vec2 value);
    void setVec3(const GLchar* name, glm::vec3 value);
    void setVec4(const GLchar* name, glm::vec4 value);
    void setMat4(const GLchar* name, glm::mat4 value);
    void setDouble(const GLchar* name, double value);

  private:
    void loadShaderSource(GLint shaderID, const char* fileName);
    static void checkShader(GLuint shaderID);
    static void checkProgram(GLuint programID);

    GLuint m_shaderMask = 0;
    std::vector<std::string> m_shaderPaths;
    std::vector<definePair> m_defines;

    GLuint m_ProgramID = 0xFFFFFFFF;
    GLint modelUniformLocation = -1;
};
