#include "CommonStructs/CommonStructs.hpp"
#include <future>

#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_opengl3.h>
#include <ImGui/imgui_internal.h>
#include <glm/common.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <stb/stb_image.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "Renderer.hpp"
#include <App/App.hpp>
#include <App/Input.hpp>
#include <utils/OpenGLErrorHandler.hpp>
#include <utils/imgui_extensions.hpp>

Renderer::Renderer(App& app) : app(app)
{
    // todo: handle errors!

    //  initialize window values
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, MSAA);
#ifdef OPENGL_DEBUG_CONTEXT
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    window = glfwCreateWindow(width, height, "Playlist Filter", nullptr, nullptr);
    glfwGetWindowPos(window, &window_off_x, &window_off_y);
    glfwGetWindowSize(window, &width, &height);
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetFramebufferSizeCallback(window, resizeCallback);
    glfwSetWindowUserPointer(window, reinterpret_cast<void*>(this));
    cam.setAspect(static_cast<float>(width) / static_cast<float>(height));

    glfwGetWindowContentScale(window, &dpiScale, nullptr);
    FONT_SIZE = static_cast<float>(FONT_SIZE) * dpiScale;

    // init opengl
    if(!gladLoadGL())
    {
        printf("Something went wrong loading OpenGL!\n");
        exit(-1);
    }

#ifdef OPENGL_DEBUG_CONTEXT
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // synch or asynch debug output
    glDebugMessageCallback(OpenGLMessageCallback, 0);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_FALSE); // DISABLE ALL MESSAGES
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, 0, GL_TRUE); // ENABLE ERRORS
    glDebugMessageControl(
        GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM, 0, 0, GL_TRUE); // ENABLE MAJOR WARNINGS
#endif
#if MSAA > 0
    glEnable(GL_MULTISAMPLE);
#endif

    // IMGUI /////////////////////////////////////////////////////////////
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::GetStyle().ScaleAllSizes(dpiScale);

#ifdef WIN32
    ImFont* defaultFont =
        io.Fonts->AddFontFromFileTTF("C:/WINDOWS/Fonts/verdana.ttf", FONT_SIZE, nullptr, nullptr);
#else
    #error No font selected for non win32 systems
#endif
    // need a font that supports all requested special symbols:
    // list of fonts supporting a symbol can be found here:
    // https://www.fileformat.info/info/unicode/char/25B6/fontsupport.htm
    // alternativly render those symbols as actual (non glyph) textures instead (should be more portable, but
    // font-based more convenient atm)
    ImVector<ImWchar> unicodeRanges;
    ImFontGlyphRangesBuilder fgrBuilder;
    fgrBuilder.AddChar(0x25B6);
    fgrBuilder.AddChar(0x21BA);
    fgrBuilder.BuildRanges(&unicodeRanges);
    // merge font instead of loading as seperate, saves Push/PopFont() calls
    ImFontConfig unicodeFontConfig;
    unicodeFontConfig.MergeMode = true;
    ImFont* unicodeFont = io.Fonts->AddFontFromFileTTF(
        MISC_PATH "/DejaVuSans.ttf", FONT_SIZE, &unicodeFontConfig, unicodeRanges.Data);
    io.Fonts->Build();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // platform/renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    // Rendering ///////////////////////////////////////////////////////

    glClearColor(.11, .11, .11, .11);
    glEnable(GL_DEPTH_TEST);

    minimalColorShader = ShaderProgram(
        VERTEX_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {SHADERS_PATH "/MinimalColor/minimalColor.vert", SHADERS_PATH "/MinimalColor/minimalColor.frag"});

    minimalVertexColorShader = ShaderProgram(
        VERTEX_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {SHADERS_PATH "/MinimalVertexColor/minimalVertexColor.vert",
         SHADERS_PATH "/MinimalVertexColor/minimalVertexColor.frag"});

    CoverGraphingShader = ShaderProgram(
        VERTEX_SHADER_BIT | GEOMETRY_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {SHADERS_PATH "/CoverGraphing/coverGraphing.vert",
         SHADERS_PATH "/CoverGraphing/coverGraphing.geom",
         SHADERS_PATH "/CoverGraphing/coverGraphing.frag"});

    glGenVertexArrays(1, &lineVAO);
    glBindVertexArray(lineVAO);
    glGenBuffers(1, &lineVBO);
    std::vector<glm::vec3> axisPoints = {
        // clang-format off
        {0.f, 0.f, 0.f},{1.f,0.f,0.f},
        {1.f, 0.f, 0.f},{1.f,0.f,0.f},
        {0.f, 0.f, 0.f},{0.f,1.f,0.f},
        {0.f, 1.f, 0.f},{0.f,1.f,0.f},
        {0.f, 0.f, 0.f},{0.f,0.f,1.f},
        {0.f, 0.f, 1.f},{0.f,0.f,1.f},
        // clang-format on
    };
    glBindBuffer(GL_ARRAY_BUFFER, lineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * axisPoints.size(), axisPoints.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, (6 * sizeof(float)), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, (6 * sizeof(float)), (void*)(3 * sizeof(float)));

    glGenVertexArrays(1, &gridVAO);
    glBindVertexArray(gridVAO);
    glGenBuffers(1, &gridVBO);
    const float subdiv = 10;
    for(int i = 0; i <= (int)subdiv; i++)
    {
        gridPoints.emplace_back(2.0f * i / subdiv - 1, 0.f, -1.f);
        gridPoints.emplace_back(2.0f * i / subdiv - 1, 0.f, 1.f);

        gridPoints.emplace_back(-1.0f, 0.f, 2.0f * i / subdiv - 1);
        gridPoints.emplace_back(1.0f, 0.f, 2.0f * i / subdiv - 1);
    }
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(decltype(gridPoints)::value_type) * gridPoints.size(),
        gridPoints.data(),
        GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    // Load Spotify Logo
    glGenTextures(1, &spotifyLogoHandle);
    glBindTexture(GL_TEXTURE_2D, spotifyLogoHandle);
    unsigned char* data = nullptr;
    {
        int x, y, components;
        data = stbi_load(MISC_PATH "/Spotify_Logo_RGB_Green.png", &x, &y, &components, 0);
        logoAspect = static_cast<float>(y) / x;

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    // Load Spotify Icon
    glGenTextures(1, &spotifyIconHandle);
    glBindTexture(GL_TEXTURE_2D, spotifyIconHandle);
    {
        int x, y, components;
        data = stbi_load(MISC_PATH "/Spotify_Icon_RGB_Green.png", &x, &y, &components, 0);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        // should switch back to default (4?) ?
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    // set time to 0 before renderloop
    last_frame = 0.0;
    glfwSetTime(0.0);

    //  draw one frame to establish ImGui Context
    // need this otherwise CalcTextSize in Table Constructor doesnt work :shrug:
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

Renderer::~Renderer()
{
    glDeleteTextures(1, &spotifyLogoHandle);
    glDeleteTextures(1, &spotifyIconHandle);
    glDeleteBuffers(1, &lineVBO);
    glDeleteVertexArrays(1, &lineVAO);
    glDeleteBuffers(1, &gridVBO);
    glDeleteVertexArrays(1, &gridVAO);

    if(renderDataWasCreated)
    {
        glDeleteTextures(1, &coverArrayHandle);
        glDeleteBuffers(1, &trackVBO);
        glDeleteVertexArrays(1, &trackVAO);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void Renderer::createRenderData()
{
    unsigned char* data = nullptr;

    // init covers array & load placeholder Album cover
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &coverArrayHandle);
    glTextureParameteri(coverArrayHandle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(coverArrayHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    // size() + 1 for the default icon
    glTextureStorage3D(coverArrayHandle, 3, GL_RGB8, 64, 64, app.getCoverTable().size() + 1);
    // Load data of placeholder texture
    {
        int x, y, components;
        data = stbi_load(MISC_PATH "/albumPlaceholder.jpg", &x, &y, &components, 3);
        // Load into first layer of array
        glTextureSubImage3D(coverArrayHandle, 0, 0, 0, 0, 64, 64, 1, GL_RGB, GL_UNSIGNED_BYTE, data);
    }
    glGenerateTextureMipmap(coverArrayHandle);
    // create image view to handle layer as indiv texture
    GLuint defaultCoverHandle;
    glGenTextures(1, &defaultCoverHandle);
    glTextureView(defaultCoverHandle, GL_TEXTURE_2D, coverArrayHandle, GL_RGB8, 0, 3, 0, 1);

    stbi_image_free(data);

    CoverInfo defaultInfo = {.url = "", .layer = 0, .id = defaultCoverHandle};
    app.getCoverTable()[""] = defaultInfo;

    for(auto& entry : app.getCoverTable())
    {
        entry.second.id = defaultCoverHandle;
    }

    glGenVertexArrays(1, &trackVAO);
    glBindVertexArray(trackVAO);
    glGenBuffers(1, &trackVBO);
    glBindBuffer(GL_ARRAY_BUFFER, trackVBO);
    GraphingBufferElement placeholder;
    glBufferData(GL_ARRAY_BUFFER, sizeof(GraphingBufferElement) * 1, &placeholder, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GraphingBufferElement), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(GraphingBufferElement), (void*)(3 * sizeof(float)));

    renderDataWasCreated = true;
}

void Renderer::uploadGraphingData(const std::vector<GraphingBufferElement>& data)
{
    glNamedBufferData(trackVBO, sizeof(data[0]) * data.size(), data.data(), GL_STATIC_DRAW);
    graphingDataCount = data.size();
};

void Renderer::highlightWindow(const std::string& name)
{
    ImGui::SetWindowFocus(name.c_str());
}

void Renderer::startFrame()
{
    glfwPollEvents();
    // start imgui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // process time, or use ImGui's delatTime
    double current_frame = glfwGetTime();
    float delta_time = current_frame - last_frame;
    last_frame = current_frame;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::drawBackgroundWindow()
{
    const ImGuiWindowFlags bgWindowFlags = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
                                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::Begin("bgWindow", nullptr, bgWindowFlags);
    {
        float fpsTextHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetCursorScreenPos(ImVec2(scaleByDPI<float>(5), height - fpsTextHeight - scaleByDPI<float>(5)));
        ImGui::Text(
            "Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);

        const ImVec2 textSize = ImGui::CalcTextSize("All data provided by");
        const ImVec2 padding(scaleByDPI(5.f), scaleByDPI(10.f));
        const ImVec2 logoSize = {scaleByDPI(100.0f), scaleByDPI(100.0f * logoAspect)};
        const ImVec2 start{width - padding.x, height - padding.y};
        ImGui::SetCursorScreenPos({start.x - logoSize.x, start.y - logoSize.y});
        ImGui::Image((void*)(intptr_t)(spotifyLogoHandle), logoSize);
        ImGui::SetCursorScreenPos({start.x - textSize.x, start.y - fpsTextHeight - logoSize.y});
        ImGui::TextUnformatted("All data provided by");
    }
    ImGui::End();
}

void Renderer::drawUI()
{
    // Finalize UI
    ImGui::Render();

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Renderer::draw3DGraph(float coverSize, glm::vec2& minMaxX, glm::vec2& minMaxY, glm::vec2& minMaxZ)
{
    // todo: move this into camera code (some update() func)
    if(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByPopup))
    {
        if(cam.mode == CAMERA_ORBIT)
        {
            if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE))
            {
                double xPos = NAN;
                double yPos = NAN;
                glfwGetCursorPos(window, &xPos, &yPos);
                auto dx = static_cast<float>(xPos - mouse_x);
                auto dy = static_cast<float>(yPos - mouse_y);
                if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                    cam.move(glm::vec3(-dx * 0.005f, dy * 0.005f, 0.f));
                else
                    cam.rotate(dx, dy);

                mouse_x = xPos;
                mouse_y = yPos;
            }
        }
        else if(cam.mode == CAMERA_FLY)
        {
            double xPos = NAN;
            double yPos = NAN;
            glfwGetCursorPos(window, &xPos, &yPos);
            auto dx = static_cast<float>(xPos - mouse_x);
            auto dy = static_cast<float>(yPos - mouse_y);
            mouse_x = xPos;
            mouse_y = yPos;
            cam.rotate(dx * 0.5f, -dy * 0.5f); // viewVector is flipped, angle diff reversed

            glm::vec3 cam_move = glm::vec3(
                static_cast<float>(
                    (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) -
                    (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)),
                static_cast<float>(
                    (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) -
                    (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)),
                static_cast<float>(
                    (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) -
                    (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)));
            if(cam_move != glm::vec3(0.0f))
                cam.move(2.0f * glm::normalize(cam_move) * ImGui::GetIO().DeltaTime);
        }
        cam.updateView();
    }

    float yoff = 2 * ((*cam.getView())[1][2] < 0) - 1;
    float zoff = 2 * ((*cam.getView())[2][2] < 0) - 1;
    float xoff = 2 * ((*cam.getView())[0][2] < 0) - 1;

    glDepthMask(GL_FALSE);
    // uniform locations are explicity set in shader
    minimalColorShader.UseProgram();
    glm::vec4 col{0.5f, 0.5f, 0.5f, 1.0f};
    glUniform4fv(4, 1, glm::value_ptr(col));
    glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(*(cam.getView())));
    glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(*(cam.getProj())));
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(glm::translate(glm::mat4(1.0f), {0, yoff, 0})));
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridPoints.size());

    glUniformMatrix4fv(
        0,
        1,
        GL_FALSE,
        // overkill way of swapping two axis, but too lazy for now
        glm::value_ptr(glm::rotate(
            glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, zoff)), glm::radians(90.0f), glm::vec3(1, 0, 0))));
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridPoints.size());

    glUniformMatrix4fv(
        0,
        1,
        GL_FALSE,
        // overkill way of swapping two axis, but too lazy for now
        glm::value_ptr(glm::rotate(
            glm::translate(glm::mat4(1.0f), glm::vec3(xoff, 0, 0)), glm::radians(90.0f), glm::vec3(0, 0, 1))));
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridPoints.size());

    glDepthMask(GL_TRUE);

    minimalVertexColorShader.UseProgram();
    // glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0)));
    glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(*(cam.getView())));
    glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(*(cam.getProj())));
    // clang-format off
        glUniformMatrix4fv(0, 1, GL_FALSE, 
            glm::value_ptr(
                glm::scale(
                    glm::translate(glm::mat4(1.0f),{-1.f,-1.f,-1.f}),
                    {2.f,2.f,2.f}
                )
            )
        );
    // clang-format on
    glBindVertexArray(lineVAO);
    glDrawArrays(GL_LINES, 0, 6);

    CoverGraphingShader.UseProgram();
    CoverGraphingShader.setFloat("width", coverSize);
    CoverGraphingShader.setVec2("minMaxX", minMaxX);
    CoverGraphingShader.setVec2("minMaxY", minMaxY);
    CoverGraphingShader.setVec2("minMaxZ", minMaxZ);
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(*(cam.getView())));
    glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(*(cam.getProj())));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, coverArrayHandle);

    glBindVertexArray(trackVAO);
    glDrawArrays(GL_POINTS, 0, graphingDataCount);
}

void Renderer::endFrame()
{
    // Swap buffer
    glfwSwapBuffers(window);
}

Renderer::Ray Renderer::getMouseRay()
{
    double mx = 0;
    double my = 0;
    glfwGetCursorPos(window, &mx, &my);

    glm::mat4 invProj = glm::inverse(*(cam.getProj()));
    glm::mat4 invView = glm::inverse(*(cam.getView()));
    float screenX = 2.f * (static_cast<float>(mx) / width) - 1.f;
    float screenY = -2.f * (static_cast<float>(my) / height) + 1.f;
    glm::vec4 clipN{screenX, screenY, 0.0f, 1.0f};
    glm::vec4 viewN = invProj * clipN;
    viewN /= viewN.w;
    glm::vec4 worldN = invView * viewN;
    glm::vec4 clipF{screenX, screenY, 1.0f, 1.0f};
    glm::vec4 viewF = invProj * clipF;
    viewF /= viewF.w;
    glm::vec4 worldF = invView * viewF;

    return {worldN, glm::normalize(worldF - worldN)};
}

bool Renderer::uploadAvailableCovers(int& progressTracker)
{
    // upload new texture data if theyre ready
    // could limit to a max amount per frame to keep frametimes more stable
    if(coverLoadQueue.empty())
        return false;

    std::lock_guard<std::mutex> lock(coverLoadQueueMutex);
    while(!coverLoadQueue.empty())
    {
        TextureLoadInfo tli = coverLoadQueue.front();
        coverLoadQueue.pop();
        // create new texture
        GLuint layerToLoadInto = coverArrayFreeIndex;
        coverArrayFreeIndex += 1;
        tli.ptr->layer = layerToLoadInto;

        assert(tli.x <= 64 && tli.y <= 64 && "Cover Image larger than texture array dimensions");
        assert(tli.data != nullptr && "Trying to upload freed data to the GPU?");
        glTextureSubImage3D(
            coverArrayHandle, 0, 0, 0, layerToLoadInto, tli.x, tli.y, 1, GL_RGB, GL_UNSIGNED_BYTE, tli.data);

        GLuint albumCoverHandle;
        glGenTextures(1, &albumCoverHandle);
        glTextureView(albumCoverHandle, GL_TEXTURE_2D, coverArrayHandle, GL_RGB8, 0, 3, layerToLoadInto, 1);
        glTextureParameteri(albumCoverHandle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(albumCoverHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        stbi_image_free(tli.data);
        // add entry to table
        tli.ptr->id = albumCoverHandle;
        progressTracker++;
    }
    // generate new mipmaps now that new covers have been added
    glGenerateTextureMipmap(coverArrayHandle);

    return true;
}