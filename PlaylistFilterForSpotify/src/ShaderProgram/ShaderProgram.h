#ifndef __CG1_TOOLS_CVK_SHADERSET_H
#define __CG1_TOOLS_CVK_SHADERSET_H

// #include <ShaderInstance/ShaderInstance.h>
#include <glad/glad.h>
#include <glm/ext.hpp>
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

typedef std::pair<std::string, std::string> definePair;

enum SHADER_BLEND_MODE
{
    opaque = 0,
    clip = 1,
    blend = 2
};

class ShaderProgram
{

  public:
    // TODO: clean up, ugg a lot of this parameter mess because of insertions and non-existance of move constructor
    ShaderProgram();
    ShaderProgram(
        GLuint shader_mask, const char** ShaderNames, std::initializer_list<definePair> defines = {},
        SHADER_BLEND_MODE blendmode = SHADER_BLEND_MODE::opaque);
    ShaderProgram(
        GLuint shader_mask, std::initializer_list<std::string> l, std::initializer_list<definePair> defines = {},
        SHADER_BLEND_MODE blendmode = SHADER_BLEND_MODE::opaque);
    ShaderProgram(
        GLuint shader_mask, const char** ShaderNames, std::string name, std::initializer_list<definePair> defines = {},
        SHADER_BLEND_MODE blendmode = SHADER_BLEND_MODE::opaque);
    ShaderProgram(
        GLuint shader_mask, std::initializer_list<std::string> l, std::string name, std::initializer_list<definePair> defines = {},
        SHADER_BLEND_MODE blendmode = SHADER_BLEND_MODE::opaque);
    virtual ~ShaderProgram();

    void GenerateShaderProgram(GLuint shader_mask, std::vector<std::string> ShaderNames);

    void UseProgram();
    GLint getUniformLocation(std::string s);
    GLuint getProgramID();

    void setInt(const GLchar* name, int value);
    void setInt2(const GLchar* name, glm::ivec2 value);
    void setUint(const GLchar* name, GLuint value);
    void setFloat(const GLchar* name, float value);
    void setDouble(const GLchar* name, double value);
    // TODO: overload setVec functions to take pointer as parameter
    void setVec2(const GLchar* name, glm::vec2 value);
    void setVec3(const GLchar* name, glm::vec3 value);
    void setVec4(const GLchar* name, glm::vec4 value);
    void setMat4(const GLchar* name, glm::mat4 value);

    std::string name;
    std::vector<ShaderInstance*> instances = {};
    GLint modelUniformLocation = 0xFFFFFFFF;
    SHADER_BLEND_MODE m_blendMode = SHADER_BLEND_MODE::opaque;

  private:
    void checkShader(GLuint shaderID);
    void checkProgram(GLuint programID);
    void loadShaderSource(GLint shaderID, const char* fileName);

    GLuint m_shaderMask;
    std::vector<std::string> m_shaderPaths;
    std::vector<definePair> m_defines;

  protected:
    GLuint m_ProgramID;
};
#endif
