#include <bit>
#include <fstream>
#include <iostream>
#include <string>

#include "ShaderProgram.h"

// ShaderNames must be in order
// Vertex, Tess_Control, Tess_Eval, Geometry, Fragment, Compute

// TODO: delete and replace in repository
// DEPRECATED: USE FUNCTIONS WITH NAME INSTEAD
ShaderProgram::ShaderProgram(
    GLuint shader_mask, std::initializer_list<std::string> l, std::initializer_list<definePair> defines)
    : m_shaderMask(shader_mask), m_shaderPaths(l), m_defines(defines)
{
    GenerateShaderProgram(shader_mask, m_shaderPaths);
}

ShaderProgram& ShaderProgram::operator=(ShaderProgram&& other) noexcept
{
    modelUniformLocation = other.modelUniformLocation;
    m_shaderMask = other.m_shaderMask;
    m_shaderPaths = std::move(other.m_shaderPaths);
    m_defines = std::move(m_defines);
    // unsure which of the two to use, swapping means the old value
    //(if it was initialized) will be glDelete'd after others scope ends
    // which is probably desired because otherwise it may be forgotten in memory
    //  m_ProgramID = other.m_ProgramID;
    //  other.m_ProgramID = 0xFFFFFFFF;
    std::swap(m_ProgramID, other.m_ProgramID);
    return *this;
}

ShaderProgram::~ShaderProgram()
{
    if(m_ProgramID != 0xFFFFFFFF)
        glDeleteProgram(m_ProgramID);
}

GLuint ShaderProgram::getProgramID()
{
    return (m_ProgramID);
}

void ShaderProgram::GenerateShaderProgram(GLuint shader_mask, std::vector<std::string> ShaderNames)
{
    GLuint vertexShaderID, tessControlShaderID, tessEvalShaderID;
    GLuint geometryShaderID, fragmentShaderID, computeShaderID;

    int next_name = 0;

    m_ProgramID = 0xFFFFFFFF;

    if(shader_mask & VERTEX_SHADER_BIT)
    {
        vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
        loadShaderSource(vertexShaderID, ShaderNames[next_name++].c_str());
        glCompileShader(vertexShaderID);
        checkShader(vertexShaderID);
    }

    if(shader_mask & TESS_CONTROL_BIT)
    {
        tessControlShaderID = glCreateShader(GL_TESS_CONTROL_SHADER);
        loadShaderSource(tessControlShaderID, ShaderNames[next_name++].c_str());
        glCompileShader(tessControlShaderID);
        checkShader(tessControlShaderID);
    }

    if(shader_mask & TESS_EVAL_BIT)
    {
        tessEvalShaderID = glCreateShader(GL_TESS_EVALUATION_SHADER);
        loadShaderSource(tessEvalShaderID, ShaderNames[next_name++].c_str());
        glCompileShader(tessEvalShaderID);
        checkShader(tessEvalShaderID);
    }

    if(shader_mask & GEOMETRY_SHADER_BIT)
    {
        geometryShaderID = glCreateShader(GL_GEOMETRY_SHADER);
        loadShaderSource(geometryShaderID, ShaderNames[next_name++].c_str());
        glCompileShader(geometryShaderID);
        checkShader(geometryShaderID);
    }

    if(shader_mask & FRAGMENT_SHADER_BIT)
    {
        fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
        loadShaderSource(fragmentShaderID, ShaderNames[next_name++].c_str());
        glCompileShader(fragmentShaderID);
        checkShader(fragmentShaderID);
    }

    if(shader_mask & COMPUTE_SHADER_BIT)
    {
        computeShaderID = glCreateShader(GL_COMPUTE_SHADER);
        loadShaderSource(computeShaderID, ShaderNames[next_name++].c_str());
        glCompileShader(computeShaderID);
        checkShader(computeShaderID);
    }

    // link shader programs
    m_ProgramID = glCreateProgram();

    if(shader_mask & VERTEX_SHADER_BIT)
        glAttachShader(m_ProgramID, vertexShaderID);
    if(shader_mask & TESS_CONTROL_BIT)
        glAttachShader(m_ProgramID, tessControlShaderID);
    if(shader_mask & TESS_EVAL_BIT)
        glAttachShader(m_ProgramID, tessEvalShaderID);
    if(shader_mask & GEOMETRY_SHADER_BIT)
        glAttachShader(m_ProgramID, geometryShaderID);
    if(shader_mask & FRAGMENT_SHADER_BIT)
        glAttachShader(m_ProgramID, fragmentShaderID);
    if(shader_mask & COMPUTE_SHADER_BIT)
        glAttachShader(m_ProgramID, computeShaderID);

    glLinkProgram(m_ProgramID);
    checkProgram(m_ProgramID);

    glUseProgram(m_ProgramID);
    modelUniformLocation = glGetUniformLocation(m_ProgramID, "model");
}

void ShaderProgram::UseProgram()
{
    glUseProgram(m_ProgramID);
}

// checks a shader for compiler errors
void ShaderProgram::checkShader(GLuint shaderID)
{
    GLint status;
    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &status);

    if(status == GL_FALSE)
    {
        GLint infoLogLength;
        glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

        GLchar* infoLog = new GLchar[infoLogLength + 1];
        glGetShaderInfoLog(shaderID, infoLogLength, NULL, infoLog);

        std::cerr << "ERROR: Unable to compile shader\n";
        std::cerr << infoLog << std::endl;
        delete[] infoLog;
    }
    else
    {
        std::cout << "SUCCESS: Shader compiled\n";
    }
}

// checks a program
void ShaderProgram::checkProgram(GLuint programID)
{
    GLint status;
    glGetProgramiv(programID, GL_LINK_STATUS, &status);

    if(status == GL_FALSE)
    {
        GLint infoLogLength;
        glGetProgramiv(programID, GL_INFO_LOG_LENGTH, &infoLogLength);

        GLchar* infoLog = new GLchar[infoLogLength + 1];
        glGetProgramInfoLog(programID, infoLogLength, NULL, infoLog);

        std::cerr << "ERROR: Unable to link ShaderSet\n";
        std::cerr << infoLog << std::endl;
        delete[] infoLog;
    }
    else
    {
        std::cout << "SUCCESS: ShaderSet linked\n";
    }
}

// reads a file and returns the content as a pointer to chars
void ShaderProgram::loadShaderSource(GLint shaderID, const char* fileName)
{
    // try to open file
    std::ifstream file(fileName);
    if(!file.is_open())
    {
        std::cerr << "ERROR: Unable to open file " << fileName << std::endl;
        return;
    }
    if(file.eof())
    {
        std::cerr << "ERROR: File is empty " << fileName << std::endl;
        return;
    }

    std::string fileNameAndExt(fileName);
    size_t lastSep = fileNameAndExt.find_last_of("/\\");
    std::string filePath = fileNameAndExt.substr(0, lastSep);
    fileNameAndExt = "\"" + fileNameAndExt.substr(lastSep, fileNameAndExt.size()) + "\"";

    // load file
    std::string fileContent;
    std::string line;

    bool versionFound = false;
    uint16_t lineNumber = 1;
    do
    {
        getline(file, line);
        lineNumber++;
        versionFound = line.substr(0, 8) == "#version";
        fileContent += line + "\n";
    }
    while(!file.eof() && !versionFound);

    fileContent += "#define GLSLANG_NDEBUG 1\n";
    for(auto& definePair : m_defines)
    {
        fileContent += "#define " + definePair.first + " " + definePair.second + "\n";
    }

    fileContent += "#line " + std::to_string(lineNumber) + " " + fileNameAndExt + "\n";

    while(!file.eof())
    {
        getline(file, line);
        fileContent += line + "\n";
    }
    file.close();

    char error[256];
    // dont need stb_include in this project
    //  char* processed = stb_include_string(fileContent.data(), "", filePath.data(), fileNameAndExt.data(),
    //  error); const char* source = processed;
    const char* source = fileContent.data();
    const GLint source_size = strlen(source);
    // if(processed == nullptr)
    // {
    //     std::cerr << "Error: Parsing File " << fileName << "\n";
    //     std::cerr << error << std::endl;
    // }

    std::cout << "SUCCESS: Opened file " << fileName << std::endl;

    glShaderSource(shaderID, 1, &source, &source_size);

    // free(processed);
}

// TODO: optimally should not use these functions since its calling glGetUniformLocation everytime
GLint ShaderProgram::getUniformLocation(std::string s)
{
    return glGetUniformLocation(m_ProgramID, s.c_str());
}

void ShaderProgram::setInt(const GLchar* name, int value)
{
    glUniform1i(glGetUniformLocation(m_ProgramID, name), value);
}

void ShaderProgram::setInt2(const GLchar* name, glm::ivec2 value)
{
    glUniform2iv(glGetUniformLocation(m_ProgramID, name), 1, glm::value_ptr(value));
}

void ShaderProgram::setUint(const GLchar* name, GLuint value)
{
    glUniform1ui(glGetUniformLocation(m_ProgramID, name), value);
}

void ShaderProgram::setDouble(const GLchar* name, double value)
{
    glUniform1d(glGetUniformLocation(m_ProgramID, name), value);
}

void ShaderProgram::setFloat(const GLchar* name, float value)
{
    glUniform1f(glGetUniformLocation(m_ProgramID, name), value);
}

void ShaderProgram::setVec2(const GLchar* name, glm::vec2 value)
{
    glUniform2fv(glGetUniformLocation(m_ProgramID, name), 1, glm::value_ptr(value));
}

void ShaderProgram::setVec3(const GLchar* name, glm::vec3 value)
{
    glUniform3fv(glGetUniformLocation(m_ProgramID, name), 1, glm::value_ptr(value));
}

void ShaderProgram::setVec4(const GLchar* name, glm::vec4 value)
{
    glUniform4fv(glGetUniformLocation(m_ProgramID, name), 1, glm::value_ptr(value));
}

void ShaderProgram::setMat4(const GLchar* name, glm::mat4 value)
{
    glUniformMatrix4fv(glGetUniformLocation(m_ProgramID, name), 1, GL_FALSE, glm::value_ptr(value));
}
