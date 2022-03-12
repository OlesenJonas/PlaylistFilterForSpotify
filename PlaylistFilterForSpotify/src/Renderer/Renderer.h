#pragma once

#include <glad/glad.h>

#include <mutex>
#include <queue>

#include <GLFW/glfw3.h>

#include "Camera/Camera.h"
#include "CommonStructs/CommonStructs.h"
#include "ShaderProgram/ShaderProgram.h"
#include "Track/Track.h"

#define MSAA 4

#ifndef NDEBUG // if not in not debug mode, ie: if in debug mode
    #define OPENGL_DEBUG_CONTEXT
#endif

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

    void drawLogIn();
    void drawPLSelect();
    void drawMain();

    void buildRenderData();
    void rebuildBuffer();
    void highlightWindow(const std::string& name);

    template <typename T>
    inline T scaleByDPI(T in) const
    {
        return in * dpiScale;
    }

    // todo: make private
    // Window Settings
    GLFWwindow* window;
    int width = 1600;
    int height = 900;
    int window_off_x = 50;
    int window_off_y = 50;
    int FONT_SIZE = 14;

    Camera cam = Camera(static_cast<float>(width) / height);
    double mouse_x = static_cast<float>(width) / 2.0f;
    double mouse_y = static_cast<float>(height) / 2.0f;

    int graphingFeature1 = 0;
    int graphingFeature2 = 1;
    int graphingFeature3 = 2;

    App& app;

    std::vector<TrackBufferElement> trackBuffer;
    float coverSize3D = 0.1f;
    Track* selectedTrack = nullptr;

    GLuint debugLinesPointBuffer = 0;
    int debugLinesPointBufferSize = 0;
    bool uiHidden = false;

  private:
    void fillTrackBuffer(int i1, int i2, int i3);

    double last_frame;

    float dpiScale = 1.0f;

    GLuint coverArrayHandle;
    GLuint coverArrayFreeIndex = 1;
    bool canLoadCovers = true;
    int coversTotal;
    int coversLoaded;
    std::mutex coverLoadQueueMutex;
    std::queue<TextureLoadInfo> coverLoadQueue;

    GLuint lineVAO;
    GLuint trackVAO;
    GLuint trackVBO;
    GLuint gridVAO;
    GLuint debugLinesVAO;
    std::vector<glm::vec3> gridPoints;
    ShaderProgram minimalShaderProgram;
    ShaderProgram minimalColorShader;
    ShaderProgram trackShader;
    ShaderProgram lineShader;
    float logoAspect = 1.0f;
    GLuint spotifyLogoHandle;
    GLuint loadingSpiralHandle;

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    const int coverSize = 40;
};