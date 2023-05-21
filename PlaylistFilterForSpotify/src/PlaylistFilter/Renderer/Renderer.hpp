#pragma once

#include <glad/glad.h>

#include <mutex>
#include <queue>

#include <GLFW/glfw3.h>

#include <Camera/Camera.hpp>
#include <CommonStructs/CommonStructs.hpp>
#include <ShaderProgram/ShaderProgram.hpp>
#include <Track/Track.hpp>

#define MSAA 4

#ifndef NDEBUG // if not in not debug mode, ie: if in debug mode
    #define OPENGL_DEBUG_CONTEXT
#endif

// #define SHOW_IMGUI_DEMO_WINDOW

class App;

class Renderer
{
  public:
    explicit Renderer(App& a);
    Renderer(const App&&) = delete;                 // prevents rvalue binding
    Renderer(const Renderer&) = delete;             // copy constr
    Renderer& operator=(const Renderer&) = delete;  // copy assign
    Renderer(Renderer&&) = delete;                  // move constr
    Renderer& operator=(Renderer&& other) = delete; // move assign
    ~Renderer();

    void startFrame();
    void drawBackgroundWindow();
    void drawUI();
    void draw3DGraph(float coverSize, glm::vec2& minMaxX, glm::vec2& minMaxY, glm::vec2& minMaxZ);
    void endFrame();

    /*
        returns true if textures were uploaded
    */
    bool uploadAvailableCovers(int& progressTracker);

    void createRenderData();
    bool renderDataWasCreated = false;
    void highlightWindow(const std::string& name);

    template <typename T>
    inline T scaleByDPI(T in) const
    {
        return in * dpiScale;
    }

    App& app;

    // Window Settings
    GLFWwindow* window;
    int width = 1600;
    int height = 900;
    int window_off_x = 50;
    int window_off_y = 50;
    double mouse_x = static_cast<float>(width) / 2.0f;
    double mouse_y = static_cast<float>(height) / 2.0f;
    Camera cam = Camera(static_cast<float>(width) / height);

    GLuint spotifyIconHandle;

    GLuint coverArrayHandle;
    GLuint coverArrayFreeIndex = 1;
    std::mutex coverLoadQueueMutex;
    std::queue<TextureLoadInfo> coverLoadQueue;

    void uploadGraphingData(const std::vector<GraphingBufferElement>& data);
    uint32_t graphingDataCount = 0;

  private:
    int FONT_SIZE = 14;
    float dpiScale = 1.0f;

    double last_frame;

    GLuint lineVAO;
    GLuint lineVBO;
    GLuint trackVAO;
    GLuint trackVBO;
    GLuint gridVAO;
    GLuint gridVBO;
    std::vector<glm::vec3> gridPoints;
    ShaderProgram minimalColorShader;
    ShaderProgram minimalVertexColorShader;
    ShaderProgram CoverGraphingShader;
    float logoAspect = 1.0f;
    GLuint spotifyLogoHandle;
};