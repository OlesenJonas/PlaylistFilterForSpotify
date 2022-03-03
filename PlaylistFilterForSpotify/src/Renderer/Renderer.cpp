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

Renderer::Renderer(App& a) : app(a)
{
}

// todo: check if already initialized
void Renderer::init()
{
    // initliaze window values
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if MSAA > 0
    glfwWindowHint(GLFW_SAMPLES, MSAA);
#endif
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

    // init opengl
    if(!gladLoadGL())
    {
        printf("Something went wrong!\n");
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

    ImFont* defaultFont = io.Fonts->AddFontFromFileTTF("C:/WINDOWS/Fonts/verdana.ttf", FONT_SIZE, NULL, NULL);

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
    // todo:
    //  not sure what to do here, maybe find freely availiable font that supports the glyphs and package with
    //  program
    ImFont* unicodeFont = io.Fonts->AddFontFromFileTTF(
        "C:/WINDOWS/Fonts/DEJAVUSANS.ttf", FONT_SIZE, &unicodeFontConfig, unicodeRanges.Data);
    io.Fonts->Build();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // platform/renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    // Rendering ///////////////////////////////////////////////////////

    glClearColor(.11, .11, .11, .11);
    glEnable(GL_DEPTH_TEST);

    minimalShaderProgram = ShaderProgram(
        VERTEX_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {EXECUTABLE_PATH "/Shaders/Minimal/minimal.vert", EXECUTABLE_PATH "/Shaders/Minimal/minimal.frag"});

    minimalColorShader = ShaderProgram(
        VERTEX_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {EXECUTABLE_PATH "/Shaders/MinimalColor/minimalColor.vert",
         EXECUTABLE_PATH "/Shaders/MinimalColor/minimalColor.frag"});

    lineShader = ShaderProgram(
        VERTEX_SHADER_BIT | GEOMETRY_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {EXECUTABLE_PATH "/Shaders/Line/line.vert",
         EXECUTABLE_PATH "/Shaders/Line/line.geom",
         EXECUTABLE_PATH "/Shaders/Line/line.frag"});

    trackShader = ShaderProgram(
        VERTEX_SHADER_BIT | GEOMETRY_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {EXECUTABLE_PATH "/Shaders/Track/track.vert",
         EXECUTABLE_PATH "/Shaders/Track/track.geom",
         EXECUTABLE_PATH "/Shaders/Track/track.frag"});

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
        gridPoints.emplace_back(2 * i / subdiv - 1, 0.f, -1.f);
        gridPoints.emplace_back(2 * i / subdiv - 1, 0.f, 1.f);

        gridPoints.emplace_back(-1.0f, 0.f, 2 * i / subdiv - 1);
        gridPoints.emplace_back(1.0f, 0.f, 2 * i / subdiv - 1);
    }
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(decltype(gridPoints)::value_type) * gridPoints.size(),
        gridPoints.data(),
        GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glGenVertexArrays(1, &debugLinesVAO);
    glBindVertexArray(debugLinesVAO);
    GLuint debugLinesVBO;
    glGenBuffers(1, &debugLinesVBO);
    debugLinesPointBuffer = debugLinesVBO;
    std::vector<glm::vec3> debugLinesPoints = {
        {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 1.0f}};
    debugLinesPointBufferSize = 4;
    glBindBuffer(GL_ARRAY_BUFFER, debugLinesVBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(glm::vec3) * debugLinesPoints.size(),
        debugLinesPoints.data(),
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
        // should switch back to default (4?) ?

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    // texture settings
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

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
    // todo: probably more stuff that i forgot to delete here

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void Renderer::draw()
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
                albumCoverHandle, GL_TEXTURE_2D, coverArrayHandle, GL_RGB8, 0, 2, layerToLoadInto, 1);

            stbi_image_free(tli.data);
            // add entry to table
            tli.ptr->id = albumCoverHandle;
            coversLoaded++;
        }
        // generate new mipmaps now that new covers have been added
        glGenerateTextureMipmap(coverArrayHandle);
        // also need to regenerate TrackBuffer, so that new layer indices are uploaded to GPU aswell
        // todo: dont need to re-fill full buffer, just update the indices -> add function that replaces just
        // these parts
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

    // not sure why [1][2] is world y of camera y (instead of [1][1])
    float yoff = 2 * ((*cam.getView())[1][2] < 0) - 1;
    float zoff = 2 * ((*cam.getView())[2][2] < 0) - 1;
    float xoff = 2 * ((*cam.getView())[0][2] < 0) - 1;

    glDepthMask(GL_FALSE);
    // uniform locations are explicity set in shader
    minimalShaderProgram.UseProgram();
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
    // Debugging selection raycast
    //  clang-format off
    glUniformMatrix4fv(
        0,
        1,
        GL_FALSE,
        glm::value_ptr(glm::translate(glm::vec3(-1.f, -1.f, -1.f)) * glm::scale(glm::vec3(2.f, 2.f, 2.f))));
    // clang-format on
    col = {0.0f, 0.5f, 0.0f, 1.0f};
    glUniform4fv(4, 1, glm::value_ptr(col));
    glBindVertexArray(debugLinesVAO);
    glDrawArrays(GL_LINES, 0, debugLinesPointBufferSize);

    minimalColorShader.UseProgram();
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

    trackShader.UseProgram();
    trackShader.setFloat("width", coverSize3D);
    trackShader.setVec2("minMaxX", app.featureMinMaxValues[graphingFeature1]);
    trackShader.setVec2("minMaxY", app.featureMinMaxValues[graphingFeature2]);
    trackShader.setVec2("minMaxZ", app.featureMinMaxValues[graphingFeature3]);
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
        ImGui::SetCursorScreenPos(ImVec2(5, height - fpsTextHeight - 5));
        // ImGui::TextUnformatted(fpsTextBuffer.data());
        ImGui::Text(
            "Application average %.3f ms/frame (%.1f FPS)",
            1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);

        const ImVec2 textSize = ImGui::CalcTextSize("All data provided by");
        const ImVec2 padding(5.f, 10.f);
        const ImVec2 logoSize = {100, 100 * logoAspect};
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
        ImGui::TextUnformatted("Filter Dataset:");
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
            float max = i != 7 ? 1.0f : 300.0f;
            float speed = i != 7 ? 0.001f : 1.0f;
            ImGui::TextUnformatted(Track::FeatureNames[i].data());
            IMGUI_ACTIVATE(
                ImGui::DragFloatRange2(
                    ("Min/Max##Feature" + std::to_string(i)).c_str(),
                    &app.featureMinMaxValues[i].x,
                    &app.featureMinMaxValues[i].y,
                    speed,
                    0.0f,
                    max),
                app.filterDirty);
            ImGui::SameLine();
            if(ImGui::Button((u8"↺##Feature" + std::to_string(i)).c_str()))
            {
                app.featureMinMaxValues[i] = {0.f, max};
                app.filterDirty = true;
            }
        }
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::Separator();
        // todo:
        // probably better to make this a contextual menu when right(or left) clicking a track in the
        // 3d view saves akward mouse movement from center view to the side of the screen
        if(selectedTrack != nullptr)
        {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
            if(ImGui::InvisibleButton("hiddenButtonSelected", ImVec2(64, 64)))
            {
                app.startTrackPlayback(selectedTrack->id);
            }
            ImVec2 afterImagePos = ImGui::GetCursorPos();
            ImGui::SameLine();
            ImVec2 textStartPos = ImGui::GetCursorPos();
            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y));
            if(ImGui::IsItemHovered())
            {
                ImVec2 textSize = ImGui::CalcTextSize(u8"▶");
                ImGui::SetCursorScreenPos(
                    ImVec2(cursorPos.x + (64 - textSize.x) * 0.5f, cursorPos.y + (64 - textSize.y) * 0.5f));
                ImGui::Text(u8"▶");
            }
            else
            {
                ImGui::Image((void*)(intptr_t)(selectedTrack->coverInfoPtr->id), ImVec2(64, 64));
            }
            // ImGui::SetCursorScreenPos(textStartPos);
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
            ImGui::SetNextItemWidth(64);
            if(ImGui::Button("Pin Track"))
            {
                app.pinTrack(selectedTrack);
            }
        }
    }
    ImGui::End();

    if(!uiHidden)
    {
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can
        // browse its code to learn more about Dear ImGui!).
        if(show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a
        // named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)
            ImGui::Checkbox(
                "Demo Window", &show_demo_window); // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if(ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true
                                        // when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text(
                "Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / ImGui::GetIO().Framerate,
                ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if(show_another_window)
        {
            ImGui::Begin(
                "Another Window",
                &show_another_window); // Pass a pointer to our bool variable (the window will have a
                                       // closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if(ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

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
                    // handle partially loaded covers (when loading cover earlier went wrong, or new tracks
                    // were added (not yet included))
                    // probably easiest to have a "isLoaded" flag as part of CoverInfo
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
                ImGui::SameLine(app.pinnedTracksTable.width - 952.f);
                if(ImGui::Button("Recommend tracks to pin"))
                {
                    app.extendPinsByRecommendations();
                }
                ImGui::SameLine(app.pinnedTracksTable.width - 790.f);
                ImGui::SetNextItemWidth(100);
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

                ImGui::SameLine(app.pinnedTracksTable.width - 189.f);
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
            ImGui::SetNextWindowSizeConstraints(ImVec2(-1, 0), ImVec2(-1, 500)); // Vertical only
            if(ImGui::Begin(
                   "Pin Recommendations", &app.showRecommendations, ImGuiWindowFlags_NoSavedSettings))
            {
                if(app.recommendedTracks.empty())
                {
                    ImGui::TextUnformatted("No recommendations found :(");
                }
                int id = -1;
                for(const auto& track : app.recommendedTracks)
                {
                    id++;
                    ImGui::PushID(id);
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    if(ImGui::InvisibleButton("hiddenButtonRecommended", ImVec2(64, 64)))
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
                            cursorPos.x + (64 - textSize.x) * 0.5f, cursorPos.y + (64 - textSize.y) * 0.5f));
                        ImGui::Text(u8"▶");
                    }
                    else
                    {
                        ImGui::Image((void*)(intptr_t)(track->coverInfoPtr->id), ImVec2(64, 64));
                    }
                    ImGui::SetCursorPos(textStartPos);
                    // TODO: fixed size (see spotify name width from table)
                    ImGui::TextUnformatted(track->trackNameEncoded.c_str());
                    ImGui::SetCursorPosX(textStartPos.x);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
                    ImGui::TextUnformatted(track->albumNameEncoded.c_str());
                    ImGui::PopStyleColor();
                    ImGui::SetCursorPosX(textStartPos.x);
                    ImGui::TextUnformatted(track->artistsNamesEncoded.c_str());

                    // TODO: get this offset from max width aswell (see above)
                    ImGui::SetCursorPosX(textStartPos.x + 150.0f);
                    ImGui::SetCursorPosY(textStartPos.y);
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