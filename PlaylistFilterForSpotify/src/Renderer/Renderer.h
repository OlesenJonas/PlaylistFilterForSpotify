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
    App& app;

    int graphingFeature1 = 0;
    int graphingFeature2 = 1;
    int graphingFeature3 = 2;

    // Window Settings
    GLFWwindow* window;
    int width = 1600;
    int height = 900;
    int window_off_x = 50;
    int window_off_y = 50;
    double mouse_x = static_cast<float>(width) / 2.0f;
    double mouse_y = static_cast<float>(height) / 2.0f;
    Camera cam = Camera(static_cast<float>(width) / height);

    float coverSize3D = 0.1f;
    std::vector<TrackBufferElement> trackBuffer;

    Track* selectedTrack = nullptr;
    bool uiHidden = false;
    ImGuiTextFilter genreFilter;

    GLuint spotifyIconHandle;

  private:
    void startFrame();
    void endFrame();
    void drawBackgroundWindow();
    void fillTrackBuffer(int i1, int i2, int i3);

    int FONT_SIZE = 14;
    float dpiScale = 1.0f;

    double last_frame;

    GLuint coverArrayHandle;
    GLuint coverArrayFreeIndex = 1;
    bool canLoadCovers = true;
    int coversTotal;
    int coversLoaded;
    std::mutex coverLoadQueueMutex;
    std::queue<TextureLoadInfo> coverLoadQueue;

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