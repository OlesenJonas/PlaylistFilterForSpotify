#include <future>

#include "glm/common.hpp"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_internal.h"
#include "stb_image.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "App/App.h"
#include "Renderer.h"
#include "utils/OpenGLErrorHandler.h"
#include "utils/imgui_extensions.h"

Renderer::Renderer(App& a) : app(a)
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
        {EXECUTABLE_PATH "/Shaders/MinimalColor/minimalColor.vert",
         EXECUTABLE_PATH "/Shaders/MinimalColor/minimalColor.frag"});

    minimalVertexColorShader = ShaderProgram(
        VERTEX_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {EXECUTABLE_PATH "/Shaders/MinimalVertexColor/minimalVertexColor.vert",
         EXECUTABLE_PATH "/Shaders/MinimalVertexColor/minimalVertexColor.frag"});

    CoverGraphingShader = ShaderProgram(
        VERTEX_SHADER_BIT | GEOMETRY_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {EXECUTABLE_PATH "/Shaders/CoverGraphing/coverGraphing.vert",
         EXECUTABLE_PATH "/Shaders/CoverGraphing/coverGraphing.geom",
         EXECUTABLE_PATH "/Shaders/CoverGraphing/coverGraphing.frag"});

    glGenVertexArrays(1, &lineVAO);
    glBindVertexArray(lineVAO);
    GLuint lineVBO;
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
    GLuint gridVBO;
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
    glDeleteTextures(1, &coverArrayHandle);
    // todo: probably more stuff that i forgot to delete here (eg all buffers)

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void Renderer::buildRenderData()
{
    unsigned char* data = nullptr;

    // init covers array & load placeholder Album cover
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &coverArrayHandle);
    glTextureParameteri(coverArrayHandle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(coverArrayHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureStorage3D(coverArrayHandle, 3, GL_RGB8, 64, 64, app.coverTable.size() + 1);
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
    app.coverTable[""] = defaultInfo;

    for(auto& entry : app.coverTable)
    {
        entry.second.id = defaultCoverHandle;
    }

    coversTotal = app.coverTable.size() - 1;
    coversLoaded = coversTotal;

    glGenVertexArrays(1, &trackVAO);
    glBindVertexArray(trackVAO);
    glGenBuffers(1, &trackVBO);
    glBindBuffer(GL_ARRAY_BUFFER, trackVBO);
    trackBuffer.reserve(app.filteredTracks.size());
    fillTrackBuffer(0, 1, 2);
    glBufferData(
        GL_ARRAY_BUFFER, sizeof(TrackBufferElement) * trackBuffer.size(), trackBuffer.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrackBufferElement), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(TrackBufferElement), (void*)(3 * sizeof(float)));
}

void Renderer::rebuildBuffer()
{
    fillTrackBuffer(graphingFeature1, graphingFeature2, graphingFeature3);
}

void Renderer::fillTrackBuffer(int i1, int i2, int i3)
{
    Track* baseptr = app.playlist.data();
    trackBuffer.clear();
    for(const Track* track : app.filteredTracks)
    {
        uint32_t index = static_cast<uint32_t>(track - baseptr);
        trackBuffer.push_back(
            {{track->features[i1], track->features[i2], track->features[i3]},
             track->coverInfoPtr->layer,
             index});
    }
    glNamedBufferData(
        trackVBO, sizeof(TrackBufferElement) * trackBuffer.size(), trackBuffer.data(), GL_STATIC_DRAW);
};

void Renderer::highlightWindow(const std::string& name)
{
    ImGui::SetWindowFocus(name.c_str());
}

void Renderer::drawLogIn()
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

    const ImGuiWindowFlags bgWindowFlags = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
                                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::Begin("bgWindow", nullptr, bgWindowFlags);
    {
        float fpsTextHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetCursorScreenPos(
            ImVec2(scaleByDPI<float>(5), height - fpsTextHeight - scaleByDPI<float>(5)));
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

    ImGui::SetNextWindowSize({60 * ImGui::CalcTextSize(u8"M").x, 0.f});
    ImGui::Begin(
        "LogIn",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    ImVec2 size = ImGui::GetWindowSize();
    ImGui::SetWindowPos(ImVec2(width / 2.0f - size.x / 2.0f, height / 2.0f - size.y / 2.0f));
    ImGui::TextWrapped("The following button will redirect you to Spotify in order"
                       "to give this application the necessary permissions. "
                       "Afterwards you will be redirected to another URL. Please "
                       "copy that URL and paste it into the following field.");
    ImGui::Dummy({0, scaleByDPI(1.0f)});
    if(ImGui::Button("Open Spotify"))
    {
        app.requestAuth();
    }
    ImGui::Dummy({0, scaleByDPI(1.0f)});
    ImGui::TextUnformatted("URL:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
    // ImGui::InputText(
    //     "##accessURL",
    //     app.userInput.data(),
    //     app.userInput.size(),
    //     ImGuiInputTextFlags_CallbackResize,
    //     ImGui::resizeUserInputVector,
    //     &app.userInput);
    ImGui::InputText("##accessURL", app.userInput.data(), app.userInput.size());
    ImGui::Dummy({0, scaleByDPI(1.0f)});
    if(ImGui::Button("Log in using URL"))
    {
        if(!app.checkAuth())
        {
            ImGui::OpenPopup("Login Error");
        }
    }
    if(ImGui::BeginPopup("Login Error"))
    {
        ImGui::Text("Error logging in using that URL\nPlease try again");
        ImGui::EndPopup();
    }
    ImGui::End();

    ImGui::Render();

    // Draw the Render Data into framebuffer
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Swap buffer
    glfwSwapBuffers(window);
}

void Renderer::drawPLSelect()
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

    const ImGuiWindowFlags bgWindowFlags = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
                                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::Begin("bgWindow", nullptr, bgWindowFlags);
    {
        float fpsTextHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetCursorScreenPos(
            ImVec2(scaleByDPI<float>(5), height - fpsTextHeight - scaleByDPI<float>(5)));
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

        if(app.loadingPlaylist)
        {
            const float barWidth = static_cast<float>(width) / 3.0f;
            const float barHeight = scaleByDPI(30.0f);
            ImGui::SetCursorScreenPos({width / 2.0f - barWidth / 2.0f, height / 2.0f});
            ImGui::HorizontalBar(0.0f, app.loadPlaylistProgress, {barWidth, barHeight});
        }
    }
    ImGui::End();

    if(!app.loadingPlaylist)
    {
        ImGui::Begin(
            "PlaylistSelection",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
        ImVec2 size = ImGui::GetWindowSize();
        ImGui::SetWindowPos(ImVec2(width / 2.0f - size.x / 2.0f, height / 2.0f - size.y / 2.0f));
        ImGui::TextUnformatted("Enter Playlist URL or ID");
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
        ImGui::TextUnformatted(
            "eg:\n- https://open.spotify.com/playlist/37i9dQZF1DX4jP4eebSWR9?si=427d9c1af97c4600\nor "
            "just:\n- 37i9dQZF1DX4jP4eebSWR9");
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(60 * ImGui::CalcTextSize(u8"M").x);
        if(ImGui::InputText("##playlistInput", app.userInput.data(), app.userInput.size() - 1))
        {
            app.playlistStatus.reset();
            app.extractPlaylistIDFromInput();
            if(!app.playlistID.empty())
            {
                app.playlistStatus = app.checkPlaylistID(app.playlistID);
            }
        }
        if(!app.playlistID.empty())
        {
            if(app.playlistStatus.has_value())
            {
                ImGui::Text("Playlist found: %s", app.playlistStatus.value().c_str());
                if(ImGui::Button("Load Playlist##selection"))
                {
                    App* test = &app;
                    app.loadingPlaylist = true;
                    app.doneLoading = std::async(std::launch::async, &App::loadSelectedPlaylist, &app);
                }
            }
            else
            {
                ImGui::Text(
                    "No Playlist found with ID: %.*s\nMake sure you have the necessary permissions",
                    static_cast<int>(app.playlistID.size()),
                    app.playlistID.data());
            }
        }
        else
        {
            ImGui::TextUnformatted("No ID found in input!");
        }
        ImGui::End();
    }
    ImGui::Render();

    // Draw the Render Data into framebuffer
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Swap buffer
    glfwSwapBuffers(window);
}

void Renderer::drawMain()
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

    // get ImGui's IO for eventual checks
    ImGuiIO& io = ImGui::GetIO();

    // upload new texture data if theyre ready
    // could limit to a max amount per frame to keep frametimes more stable
    if(!coverLoadQueue.empty())
    {
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
                coverArrayHandle,
                0,
                0,
                0,
                layerToLoadInto,
                tli.x,
                tli.y,
                1,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                tli.data);

            GLuint albumCoverHandle;
            glGenTextures(1, &albumCoverHandle);
            glTextureView(
                albumCoverHandle, GL_TEXTURE_2D, coverArrayHandle, GL_RGB8, 0, 3, layerToLoadInto, 1);
            glTextureParameteri(albumCoverHandle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(albumCoverHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

            stbi_image_free(tli.data);
            // add entry to table
            tli.ptr->id = albumCoverHandle;
            coversLoaded++;
        }
        // generate new mipmaps now that new covers have been added
        glGenerateTextureMipmap(coverArrayHandle);
        // also need to regenerate TrackBuffer, so that new layer indices are uploaded to GPU aswell
        // todo: dont need to re-fill full buffer, just update the indices -> add function that replaces
        // just these parts
        fillTrackBuffer(graphingFeature1, graphingFeature2, graphingFeature3);
    }

    // todo: move this into camera code (some update() func)
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
            cam.move(2.0f * glm::normalize(cam_move) * io.DeltaTime);
    }
    cam.updateView();

    // clear last frame
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
            glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, zoff)),
            glm::radians(90.0f),
            glm::vec3(1, 0, 0))));
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridPoints.size());

    glUniformMatrix4fv(
        0,
        1,
        GL_FALSE,
        // overkill way of swapping two axis, but too lazy for now
        glm::value_ptr(glm::rotate(
            glm::translate(glm::mat4(1.0f), glm::vec3(xoff, 0, 0)),
            glm::radians(90.0f),
            glm::vec3(0, 0, 1))));
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
    CoverGraphingShader.setFloat("width", coverSize3D);
    CoverGraphingShader.setVec2("minMaxX", app.featureMinMaxValues[graphingFeature1]);
    CoverGraphingShader.setVec2("minMaxY", app.featureMinMaxValues[graphingFeature2]);
    CoverGraphingShader.setVec2("minMaxZ", app.featureMinMaxValues[graphingFeature3]);
    glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
    glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(*(cam.getView())));
    glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(*(cam.getProj())));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, coverArrayHandle);

    glBindVertexArray(trackVAO);
    glDrawArrays(GL_POINTS, 0, trackBuffer.size());

    const ImGuiWindowFlags bgWindowFlags = ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar |
                                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar;
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::Begin("bgWindow", nullptr, bgWindowFlags);
    {
        float fpsTextHeight = ImGui::GetTextLineHeightWithSpacing();
        ImGui::SetCursorScreenPos(ImVec2(scaleByDPI(5.0f), height - fpsTextHeight - scaleByDPI(5.0f)));
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

    // SECTION: Graphing Settings
    static constexpr char* comboNames = Track::FeatureNamesData;
    ImGui::Begin(
        "Graphing Settings",
        nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
    {
        auto size = ImGui::GetWindowSize();
        ImGui::SetWindowPos(ImVec2(width - size.x, 0));

        ImGui::SliderFloat("Cover Size", &coverSize3D, 0.0f, 0.1f);
        ImGui::Separator();
        IMGUI_ACTIVATE(ImGui::Combo("X Axis Value", &graphingFeature1, comboNames), app.graphingDirty);
        IMGUI_ACTIVATE(ImGui::Combo("Y Axis Value", &graphingFeature2, comboNames), app.graphingDirty);
        IMGUI_ACTIVATE(ImGui::Combo("Z Axis Value", &graphingFeature3, comboNames), app.graphingDirty);
        ImGui::Separator();
        ImGui::TextUnformatted("Filter Playlist:");
        ImGui::Text("Track or Artist name");
        if(ImGui::InputText("##filterInput", app.stringFilterBuffer.data(), app.stringFilterBuffer.size()))
        {
            app.filterDirty = true;
        }
        ImGui::SameLine();
        if(ImGui::Button("X##text"))
        {
            app.stringFilterBuffer.fill('\0');
            app.filterDirty = true;
        }
        for(auto i = 0; i < Track::featureAmount; i++)
        {
            ImGui::PushID(i);
            float max = i != 7 ? 1.0f : 300.0f;
            float speed = i != 7 ? 0.001f : 1.0f;
            ImGui::TextUnformatted(Track::FeatureNames[i].data());
            IMGUI_ACTIVATE(
                ImGui::DragFloatRange2(
                    "Min/Max",
                    &app.featureMinMaxValues[i].x,
                    &app.featureMinMaxValues[i].y,
                    speed,
                    0.0f,
                    max),
                app.filterDirty);
            ImGui::SameLine();
            if(ImGui::Button(u8"↺"))
            {
                app.featureMinMaxValues[i] = {0.f, max};
                app.filterDirty = true;
            }
            ImGui::PopID();
        }
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::Separator();
        if(selectedTrack != nullptr)
        {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            const float coverSize = scaleByDPI(64.0f);
            if(ImGui::InvisibleButton("hiddenButtonSelected", ImVec2(coverSize, coverSize)))
            {
                app.startTrackPlayback(selectedTrack->id);
            }
            ImVec2 afterImagePos = ImGui::GetCursorPos();
            ImGui::SameLine();
            ImVec2 textStartPos = ImGui::GetCursorPos();
            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y));
            if(ImGui::IsItemHovered())
            {
                float padFactor = 0.25f;
                ImGui::SetCursorScreenPos(
                    ImVec2(cursorPos.x + padFactor * coverSize, cursorPos.y + padFactor * coverSize));
                float scaledCoverSize = (1.0f - 2.0f * padFactor) * coverSize;
                ImGui::Image((void*)(intptr_t)(spotifyIconHandle), ImVec2(scaledCoverSize, scaledCoverSize));
            }
            else
            {
                ImGui::Image(
                    (void*)(intptr_t)(selectedTrack->coverInfoPtr->id), ImVec2(coverSize, coverSize));
            }
            ImGui::SetCursorPos(textStartPos);
            // ImGui::TextUnformatted(selectedTrack->trackNameEncoded.c_str());
            ImGui::TextWrapped(selectedTrack->trackNameEncoded.c_str());
            ImGui::SetCursorPosX(textStartPos.x);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
            // ImGui::TextUnformatted(selectedTrack->albumNameEncoded.c_str());
            ImGui::TextWrapped(selectedTrack->albumNameEncoded.c_str());
            ImGui::PopStyleColor();
            ImGui::SetCursorPosX(textStartPos.x);
            // ImGui::TextUnformatted(selectedTrack->artistsNamesEncoded.c_str());
            ImGui::TextWrapped(selectedTrack->artistsNamesEncoded.c_str());

            ImGui::SetCursorPos(afterImagePos);
            ImGui::SetNextItemWidth(coverSize);
            if(ImGui::Button("Pin Track"))
            {
                app.pinTrack(selectedTrack);
            }
        }
    }
    ImGui::End();

    if(!uiHidden)
    {

#ifdef SHOW_IMGUI_DEMO_WINDOW
        static bool show_demo_window = true;
        static bool show_another_window = false;
        static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        ImGui::ShowDemoWindow(&show_demo_window);
#endif

        if(ImGui::Begin("Playlist Data", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if(ImGui::Button("Stop Playback"))
            {
                app.stopPlayback();
            }
            if(canLoadCovers)
            {
                ImGui::SameLine();
                if(ImGui::Button("Load Covers"))
                {
                    canLoadCovers = false;
                    auto getCoverData = [&](std::pair<const std::string, CoverInfo>& entry) -> void
                    {
                        // todo: error handling for all the requests
                        TextureLoadInfo tli;
                        tli.ptr = &entry.second;

                        const std::string& imageUrl = entry.second.url;
                        cpr::Response r = cpr::Get(cpr::Url(imageUrl));
                        int temp;
                        tli.data = stbi_load_from_memory(
                            (unsigned char*)r.text.c_str(), r.text.size(), &tli.x, &tli.y, &temp, 3);

                        std::lock_guard<std::mutex> lock(coverLoadQueueMutex);
                        coverLoadQueue.push(tli);
                    };
                    coversLoaded = 0;
                    // todo:
                    // handle partially loaded covers (when loading cover earlier went wrong, or new
                    // tracks were added (not yet included)) probably easiest to have a "isLoaded" flag as
                    // part of CoverInfo
                    for(std::pair<const std::string, CoverInfo>& entry : app.coverTable)
                    {
                        // skip the default texture entry
                        if(entry.first != "")
                        {
                            // c++ cant guarantee that reference stays alive, need to explicitly wrap
                            // as reference when passing to thread
                            std::thread{getCoverData, std::ref(entry)}.detach();
                        }
                    }
                }
            }
            if(coversLoaded != coversTotal)
            {
                ImGui::ProgressBar(static_cast<float>(coversLoaded) / coversTotal);
            }
            ImGui::Separator();

            app.pinnedTracksTable.draw();
            if(!app.pinnedTracks.empty())
            {
                if(ImGui::Button("Export to playlist##pins"))
                {
                    // todo: promt popup to ask for PL name
                    app.createPlaylist(app.pinnedTracks);
                }

                // todo: un-hardcode these offsets (depend on text (-> button/sliders) sizes)
                ImGui::SameLine(app.pinnedTracksTable.width - scaleByDPI(952.f));
                if(ImGui::Button("Recommend tracks to pin"))
                {
                    app.extendPinsByRecommendations();
                }
                ImGui::SameLine(app.pinnedTracksTable.width - scaleByDPI(790.f));
                ImGui::SetNextItemWidth(scaleByDPI(100.0f));
                ImGui::SliderInt("Accuracy", &app.recommendAccuracy, 1, 5, "");
                ImGui::SameLine();
                ImGui::TextDisabled("(?)");
                if(ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                    ImGui::TextUnformatted("Controls how accurate the recommendations will fit the pinned "
                                           "tracks. Higher accuracy may give less results overall.");
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }

                ImGui::SameLine(app.pinnedTracksTable.width - scaleByDPI(189.f));
                if(ImGui::Button("Create filters from pinned tracks"))
                {
                    // todo: app.XYZ(vector<Track*> v) that fills filter
                    app.featureMinMaxValues.fill(
                        glm::vec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::min()));
                    for(const auto& trackPtr : app.pinnedTracks)
                    {
                        for(auto indx = 0; indx < trackPtr->features.size(); indx++)
                        {
                            app.featureMinMaxValues[indx].x =
                                std::min(app.featureMinMaxValues[indx].x, trackPtr->features[indx]);
                            app.featureMinMaxValues[indx].y =
                                std::max(app.featureMinMaxValues[indx].y, trackPtr->features[indx]);
                        }
                    }
                    app.filterDirty = true;
                }
            }
            ImGui::Separator();

            app.filteredTracksTable.draw();
            if(ImGui::Button("Export to playlist"))
            {
                // todo: promt popup to ask for PL name
                app.createPlaylist(app.filteredTracks);
            }
        }
        ImGui::End(); // Playlist Data Window

        if(app.showRecommendations)
        {
            ImGui::SetNextWindowSize(ImVec2(0, 0));
            ImGui::SetNextWindowSizeConstraints(
                ImVec2(-1, 0), ImVec2(-1, scaleByDPI(500.0f))); // Vertical only
            if(ImGui::Begin(
                   "Pin Recommendations", &app.showRecommendations, ImGuiWindowFlags_NoSavedSettings))
            {
                if(app.recommendedTracks.empty())
                {
                    ImGui::TextUnformatted("No recommendations found :(");
                }
                int id = -1;
                for(const auto& recommendation : app.recommendedTracks)
                {
                    const float coverSize = scaleByDPI(64.0f);
                    const auto& track = recommendation.track;
                    id++;
                    ImGui::PushID(id);
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    if(ImGui::InvisibleButton("hiddenButtonRecommended", ImVec2(coverSize, coverSize)))
                    {
                        app.startTrackPlayback(track->id);
                    }
                    ImVec2 afterImagePos = ImGui::GetCursorPos();
                    ImGui::SameLine();
                    ImVec2 textStartPos = ImGui::GetCursorPos();
                    ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y));
                    if(ImGui::IsItemHovered())
                    {
                        ImVec2 textSize = ImGui::CalcTextSize(u8"▶");
                        ImGui::SetCursorScreenPos(ImVec2(
                            cursorPos.x + (coverSize - textSize.x) * 0.5f,
                            cursorPos.y + (coverSize - textSize.y) * 0.5f));
                        ImGui::Text(u8"▶");
                    }
                    else
                    {
                        ImGui::Image(
                            (void*)(intptr_t)(track->coverInfoPtr->id), ImVec2(coverSize, coverSize));
                    }
                    float maxTextSize = ImGui::CalcTextSize(u8"MMMMMMMMMMMMMMMMMMMMMMMMM").x;
                    ImGui::SetCursorPos(textStartPos);
                    if(ImGui::BeginChild("Names", ImVec2(maxTextSize, coverSize)))
                    {
                        ImGui::TextUnformatted(track->trackNameEncoded.c_str());
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
                        ImGui::TextUnformatted(track->albumNameEncoded.c_str());
                        ImGui::PopStyleColor();
                        ImGui::TextUnformatted(track->artistsNamesEncoded.c_str());
                    }
                    ImGui::EndChild(); // Names Child

                    ImGui::SameLine();
                    if(ImGui::Button("Pin Track"))
                    {
                        app.pinTrack(track);
                    }
                    ImGui::PopID();
                    ImGui::SetCursorPos(afterImagePos);
                }
            }
            ImGui::End(); // Pin Recommendations Window
        }
    }
    // Create ImGui Render Data
    ImGui::Render();

    // Draw the Render Data into framebuffer
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Swap buffer
    glfwSwapBuffers(window);
}