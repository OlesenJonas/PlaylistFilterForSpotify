#include <glad/glad.h>

#include <cstdio>
#include <execution>
#include <iterator>
#include <limits>
#include <mutex>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_internal.h"
#include "stb_image.h"

#include "Camera/Camera.h"
#include "ShaderProgram/ShaderProgram.h"
#include "Spotify/SpotifyApiAccess.h"
#include "Spotify/TrackData.h"
#include "utils/OpenGLErrorHandler.h"
#include "utils/imgui_extensions.h"
#include "utils/utf.h"

#define MSAA 4

#define IMGUI_ACTIVATE(X, B)                                                                                 \
    if(X)                                                                                                    \
    {                                                                                                        \
        B = true;                                                                                            \
    }

// todo: class/struct App that contains all the below
//  screen
int width = 1600, height = 900, window_off_x = 50, window_off_y = 50;
int FONT_SIZE = 14;

double mouse_x = static_cast<float>(width) / 2.0f;
double mouse_y = static_cast<float>(height) / 2.0f;

GLFWwindow* window;

struct TrackBufferObject
{
    glm::vec3 p;
    GLuint layer;
    // need this index for selection, when raycasting against elements in this buffer
    GLuint originalIndex;
};
std::vector<TrackData>* playlistDataPtr = nullptr;
std::vector<TrackBufferObject>* trackBufferPtr = nullptr;
TrackData* selectedTrack = nullptr;
GLuint debugLinesPointBuffer = 0;
int debugLinesPointBufferSize = 0;
float coverSize3D = 0.1f;
int graphingFeature1 = 0;
int graphingFeature2 = 1;
int graphingFeature3 = 2;
std::array<glm::vec2, TrackData::featureAmount> featureMinMaxValues;

struct Editor
{
    Camera cam = Camera(static_cast<float>(width) / height);
    bool uiHidden = false;
};

Editor editor;

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if((button == GLFW_MOUSE_BUTTON_MIDDLE || button == GLFW_MOUSE_BUTTON_RIGHT) && action == GLFW_PRESS)
    {
        glfwGetCursorPos(window, &mouse_x, &mouse_y);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        editor.cam.setMode(CAMERA_FLY);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        editor.cam.setMode(CAMERA_ORBIT);
    }
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
            return;
        double mx = 0;
        double my = 0;
        glfwGetCursorPos(window, &mx, &my);
        // TODO: put in func
        glm::mat4 invProj = glm::inverse(*(editor.cam.getProj()));
        glm::mat4 invView = glm::inverse(*(editor.cam.getView()));
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

        // everything gets remapped [0,1] -> [-1,1] in VS
        // coverBuffer contains values in [0,1] but we want to test against visible tris
        worldF = 0.5f * worldF + 0.5f;
        worldN = 0.5f * worldN + 0.5f;

        glm::vec3 rayStart = worldN;
        glm::vec3 rayDir = (worldF - worldN);
        rayDir = glm::normalize(rayDir);

        // glm::vec3 worldCamX = glm::vec3(invView * glm::vec4(coverSize3D, 0.f, 0.f, 0.f));
        // glm::vec3 worldCamY = glm::vec3(invView * glm::vec4(0.f, coverSize3D, 0.f, 0.f));
        glm::vec3 worldCamX = glm::vec3(invView * glm::vec4(1.0f, 0.f, 0.f, 0.f));
        glm::vec3 worldCamY = glm::vec3(invView * glm::vec4(0.f, 1.0f, 0.f, 0.f));
        // need worldCamX/Y later, cant just do invView*(0,0,-1,0) here
        glm::vec3 n = glm::normalize(glm::cross(worldCamX, worldCamY));

        glm::vec3 axisMins{
            featureMinMaxValues[graphingFeature1].x,
            featureMinMaxValues[graphingFeature2].x,
            featureMinMaxValues[graphingFeature3].x};
        glm::vec3 axisMaxs{
            featureMinMaxValues[graphingFeature1].y,
            featureMinMaxValues[graphingFeature2].y,
            featureMinMaxValues[graphingFeature3].y};
        glm::vec3 axisFactors = axisMaxs - axisMins;

        glm::vec3 hitP;
        glm::vec3 debugHitP;
        glm::vec3 resP;
        struct HitResult
        {
            float t = std::numeric_limits<float>::max();
            uint32_t index = std::numeric_limits<uint32_t>::max();
        };
        HitResult hit;

        for(const auto& trackBufferObject : *trackBufferPtr)
        {
            glm::vec3 tboP = trackBufferObject.p;
            tboP = (tboP - axisMins) / axisFactors;
            float t = glm::dot(tboP - rayStart, n) / glm::dot(rayDir, n);
            t = std::max(0.f, t);
            hitP = rayStart + t * rayDir;

            float localX = glm::dot(hitP - tboP, worldCamX);
            float localY = glm::dot(hitP - tboP, worldCamY);
            bool insideSquare =
                std::abs(localX) < 0.5f * coverSize3D && std::abs(localY) < 0.5f * coverSize3D;
            if(insideSquare && t < hit.t)
            {
                hit.t = t;
                hit.index = trackBufferObject.originalIndex;
                resP = tboP;
                debugHitP = hitP;
            }
        }
        selectedTrack = nullptr;
        if(hit.index != std::numeric_limits<uint32_t>::max())
        {
            selectedTrack = &((*playlistDataPtr)[hit.index]);

            // TODO: ifdef this out, when debugging selection isnt needed
            std::vector<glm::vec3> newData = {worldN, worldF, debugHitP, resP};
            glNamedBufferData(
                debugLinesPointBuffer, sizeof(glm::vec3) * newData.size(), newData.data(), GL_STATIC_DRAW);
            debugLinesPointBufferSize = newData.size();
        }
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if(key == GLFW_KEY_TAB && action == GLFW_PRESS)
        editor.uiHidden = !editor.uiHidden;
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    if(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        if(editor.cam.mode == CAMERA_ORBIT)
            editor.cam.changeRadius(yoffset < 0);
        else if(editor.cam.mode == CAMERA_FLY)
        {
            float factor = (yoffset > 0) ? 1.1f : 1 / 1.1f;
            editor.cam.flySpeed *= factor;
        }
    }
}

void resizeCallback(GLFWwindow* window, int w, int h)
{
    width = w;
    height = h;
    editor.cam.setAspect(static_cast<float>(w) / static_cast<float>(h));
    glViewport(0, 0, w, h);
}

#ifndef NDEBUG // if not in not debug mode ie: if in debug mode
    #define OPENGL_DEBUG_CONTEXT
#endif

int main()
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
    // TODO: re-enable docking (need change branch)
    //  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //  // For viewports see: "Steps to use multi-viewports in your application, when using a default back-end
    //  from the examples/ folder:" in imgui.h io.ConfigDockingWithShift = false;

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
    ImFont* unicodeFont = io.Fonts->AddFontFromFileTTF(
        "C:/WINDOWS/Fonts/DEJAVUSANS.ttf", FONT_SIZE, &unicodeFontConfig, unicodeRanges.Data);
    io.Fonts->Build();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    // platform/renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Rendering ///////////////////////////////////////////////////////

    glClearColor(.11, .11, .11, .11);
    glEnable(GL_DEPTH_TEST);

    const char* shadernames[2] = {
        EXECUTABLE_PATH "/Shaders/Minimal/minimal.vert", EXECUTABLE_PATH "/Shaders/Minimal/minimal.frag"};
    ShaderProgram minimalShaderProgram(VERTEX_SHADER_BIT | FRAGMENT_SHADER_BIT, shadernames);

    ShaderProgram minimalColorShader(
        VERTEX_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {EXECUTABLE_PATH "/Shaders/MinimalColor/minimalColor.vert",
         EXECUTABLE_PATH "/Shaders/MinimalColor/minimalColor.frag"});

    ShaderProgram lineShader(
        VERTEX_SHADER_BIT | GEOMETRY_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {EXECUTABLE_PATH "/Shaders/Line/line.vert",
         EXECUTABLE_PATH "/Shaders/Line/line.geom",
         EXECUTABLE_PATH "/Shaders/Line/line.frag"});

    ShaderProgram trackShader(
        VERTEX_SHADER_BIT | GEOMETRY_SHADER_BIT | FRAGMENT_SHADER_BIT,
        {EXECUTABLE_PATH "/Shaders/Track/track.vert",
         EXECUTABLE_PATH "/Shaders/Track/track.geom",
         EXECUTABLE_PATH "/Shaders/Track/track.frag"});

    // use the shader program
    minimalShaderProgram.UseProgram();

    GLuint lineVAO;
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

    GLuint gridVAO;
    glGenVertexArrays(1, &gridVAO);
    glBindVertexArray(gridVAO);
    GLuint gridVBO;
    glGenBuffers(1, &gridVBO);
    std::vector<glm::vec3> gridPoints;
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

    GLuint debugLinesVAO;
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
    GLuint spotifyLogoHandle;
    float logoAspect = 1.0f;
    {
        int x, y, components;
        unsigned char* data = stbi_load(MISC_PATH "/Spotify_Logo_RGB_Green.png", &x, &y, &components, 0);
        logoAspect = static_cast<float>(y) / x;
        // create new texture
        glGenTextures(1, &spotifyLogoHandle);
        glBindTexture(GL_TEXTURE_2D, spotifyLogoHandle);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        // should switch pack to default (4?) ?

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        // texture settings
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);

        stbi_image_free(data);
    }

    // Spotify Api //////////////////////////////////////////////////
    auto apiAccess = SpotifyApiAccess();
    const std::string playlist_test_id = "4yDYkPpEix7s5HK5ZBd7lz"; // art pop
    // const std::string playlist_test_id = "0xmNlq3D0z3Dxkt0T0mqyj"; // liked
    std::vector<TrackData> playlistData;
    playlistDataPtr = &playlistData;
    std::unordered_map<std::string, CoverInfo> coverTable;
    // have to use std::tie for now since CLANG doesnt allow for structured bindings to be captured in lambda
    // switch back as soon as lambda refactored into function
    std::tie(playlistData, coverTable) = apiAccess.buildPlaylistData(playlist_test_id);
    // auto [playlistData, coverTable] = apiAccess.buildPlaylistData(playlist_test_id);

    // init covers array & default cover Load Default Album cover
    GLuint coverArrayHandle;
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &coverArrayHandle);
    glTextureParameteri(coverArrayHandle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(coverArrayHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureStorage3D(coverArrayHandle, 3, GL_RGB8, 64, 64, coverTable.size() + 1);
    // Load data of default texture
    int x, y, components;
    unsigned char* data = stbi_load(MISC_PATH "/albumPlaceholder.jpg", &x, &y, &components, 3);
    // Load into first layer of array
    glTextureSubImage3D(coverArrayHandle, 0, 0, 0, 0, 64, 64, 1, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateTextureMipmap(coverArrayHandle);
    // create image view to handle layer as indiv texture
    GLuint defaultCoverHandle;
    glGenTextures(1, &defaultCoverHandle);
    glTextureView(defaultCoverHandle, GL_TEXTURE_2D, coverArrayHandle, GL_RGB8, 0, 3, 0, 1);

    stbi_image_free(data);

    CoverInfo defaultInfo = {.url = "", .layer = 0, .id = defaultCoverHandle};
    coverTable[""] = defaultInfo;

    for(auto& entry : coverTable)
    {
        entry.second.id = defaultCoverHandle;
    }

    std::vector<TrackData*> originalPlaylistData(playlistData.size());

    bool doFirstFrameCalculations = true;
    constexpr int coverSize = 40;
    float tableWidth = 0.f;
    struct ColumnHeader
    {
        std::string name;
        ImGuiTableColumnFlags flags;
        float width;
    };
    ImGuiTableColumnFlags defaultColumnFlag =
        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize;
    ImGuiTableColumnFlags noSortColumnFlag = defaultColumnFlag | ImGuiTableColumnFlags_NoSort;
    std::vector<ColumnHeader> columnHeaders = {
        {"#", defaultColumnFlag, 40.f},
        {"", noSortColumnFlag, 40.f},
        {"Name", noSortColumnFlag, 60.f},
        {"Artist(s)", noSortColumnFlag, 60.f}};
    for(const std::string_view& name : TrackData::FeatureNames)
    {
        columnHeaders.push_back({name.data(), defaultColumnFlag, 60.0f});
    }
    featureMinMaxValues.fill(glm::vec2(0.0f, 1.0f));
    featureMinMaxValues[7] = {0, 300};

    for(auto i = 0; i < playlistData.size(); i++)
    {
        originalPlaylistData[i] = &playlistData[i];
    }
    // initially the filtered playlist is the same as the original
    auto filteredPlaylistData = originalPlaylistData;
    std::vector<TrackData*> pinnedTracks = {};

    // todo: not sure if 100 is enough, change if needed (also need to adjust size in Imgui function)
    std::array<char, 100> stringFilterBuffer{};

    int lastPlayedTrack = -1;

    int columnToSortBy = 0;
    bool sortAscending = false;
    auto sortLambda = [&columnToSortBy](const TrackData* td1, const TrackData* td2) -> bool
    {
        switch(columnToSortBy)
        {
        case 0:
            return td1->index < td2->index;
        case 4 ... 12:
            return td1->features[columnToSortBy - 4] < td2->features[columnToSortBy - 4];
        default:
            assert(0 && "Column Sorting not handled");
        }
        return td1->features[7] < td2->features[7]; // shouldnt be reached
    };

    int coversTotal = coverTable.size() - 1;
    int coversLoaded = coversTotal;
    struct TextureLoadInfo
    {
        int x;
        int y;
        unsigned char* data;
        CoverInfo* ptr;
    };
    std::mutex coverLoadQueueMutex;
    std::queue<TextureLoadInfo> coverLoadQueue;

    GLuint trackVAO;
    glGenVertexArrays(1, &trackVAO);
    glBindVertexArray(trackVAO);
    GLuint trackVBO;
    glGenBuffers(1, &trackVBO);
    glBindBuffer(GL_ARRAY_BUFFER, trackVBO);
    std::vector<TrackBufferObject> trackBuffer;
    trackBufferPtr = &trackBuffer;
    trackBuffer.reserve(filteredPlaylistData.size());
    auto fillTrackBuffer = [&](int i1, int i2, int i3) -> void
    {
        TrackData* baseptr = playlistData.data();
        trackBuffer.clear();
        for(const TrackData* track : filteredPlaylistData)
        {
            uint32_t index = static_cast<uint32_t>(track - baseptr);
            trackBuffer.push_back(
                {{track->features[i1], track->features[i2], track->features[i3]},
                 track->coverInfoPtr->layer,
                 index});
        }
        glNamedBufferData(
            trackVBO, sizeof(TrackBufferObject) * trackBuffer.size(), trackBuffer.data(), GL_STATIC_DRAW);
    };
    fillTrackBuffer(0, 1, 2);
    glBufferData(
        GL_ARRAY_BUFFER, sizeof(TrackBufferObject) * trackBuffer.size(), trackBuffer.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrackBufferObject), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(TrackBufferObject), (void*)(3 * sizeof(float)));

    // set time to 0 before renderloop
    double last_frame = 0.0;
    glfwSetTime(0.0);
    // renderloop
    while(!glfwWindowShouldClose(window))
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

        // Doing calculations that require a valid context
        if(doFirstFrameCalculations)
        {
            const int pad = 10;
            for(auto i = 4; i < columnHeaders.size(); i++)
            {
                auto& header = columnHeaders[i];
                // For the audio features we can assume that the header will be wieder than any value
                header.width = pad + ImGui::CalcTextSize(header.name.c_str()).x;
            }
            // Add additional padding to last column (compensate for scrollbar)
            columnHeaders[columnHeaders.size() - 1].width += 15.f;
            // calculate max size of all other attributes
            for(const auto& track : playlistData)
            {
                ImVec2 size;
                size = ImGui::CalcTextSize(track.trackNameEncoded.c_str());
                columnHeaders[2].width = std::max(columnHeaders[2].width, size.x);
                size = ImGui::CalcTextSize(track.albumNameEncoded.c_str());
                columnHeaders[2].width = std::max(columnHeaders[2].width, size.x);
                size = ImGui::CalcTextSize(track.artistsNamesEncoded.c_str());
                columnHeaders[3].width = std::max(columnHeaders[2].width, size.x);
            }
            // todo:
            // dont just hardcode, find a representative string of this length and calculate the length
            // of that with the current font & fontsize
            // spotify gives some guidelines for length of names etc:
            // https://developer.spotify.com/documentation/general/design-and-branding/
            // Playlist/album name: 25 characters
            // Artist name: 18 characters
            // Track name: 23 characters
            float maxNameLength = 250;
            columnHeaders[2].width = std::min(columnHeaders[2].width, maxNameLength);
            columnHeaders[3].width = std::min(columnHeaders[3].width, maxNameLength);
            auto lambda = [](float f, const ColumnHeader& b) -> float
            {
                return f + b.width;
            };
            tableWidth = std::accumulate(columnHeaders.begin(), columnHeaders.end(), 0.0f, lambda);
            doFirstFrameCalculations = false;
        }

        // upload new texture data if theyre ready
        if(!coverLoadQueue.empty())
        {
            std::lock_guard<std::mutex> lock(coverLoadQueueMutex);
            while(!coverLoadQueue.empty())
            {
                TextureLoadInfo tli = coverLoadQueue.front();
                coverLoadQueue.pop();
                // create new texture
                GLuint layerToLoadInto = tli.ptr->layer;

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
            glGenerateTextureMipmap(coverArrayHandle);
        }

        if(editor.cam.mode == CAMERA_ORBIT)
        {
            if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE))
            {
                double xPos = NAN;
                double yPos = NAN;
                glfwGetCursorPos(window, &xPos, &yPos);
                auto dx = static_cast<float>(xPos - mouse_x);
                auto dy = static_cast<float>(yPos - mouse_y);
                if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
                    editor.cam.move(glm::vec3(-dx * 0.005f, dy * 0.005f, 0.f));
                else
                    editor.cam.rotate(dx, dy);

                mouse_x = xPos;
                mouse_y = yPos;
            }
        }
        else if(editor.cam.mode == CAMERA_FLY)
        {
            double xPos = NAN;
            double yPos = NAN;
            glfwGetCursorPos(window, &xPos, &yPos);
            auto dx = static_cast<float>(xPos - mouse_x);
            auto dy = static_cast<float>(yPos - mouse_y);
            mouse_x = xPos;
            mouse_y = yPos;
            editor.cam.rotate(dx * 0.5f, -dy * 0.5f); // viewVector is flipped, angle diff reversed

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
                editor.cam.move(2.0f * glm::normalize(cam_move) * io.DeltaTime);
        }
        editor.cam.updateView();

        // clear last frame
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // not sure why [1][2] is world y of camera y (instead of [1][1])
        // maybe something wrong internally
        float yoff = 2 * ((*editor.cam.getView())[1][2] < 0) - 1;
        float zoff = 2 * ((*editor.cam.getView())[2][2] < 0) - 1;
        float xoff = 2 * ((*editor.cam.getView())[0][2] < 0) - 1;

        glDepthMask(GL_FALSE);
        // uniform locations are explicity set in shader
        minimalShaderProgram.UseProgram();
        glm::vec4 col{0.5f, 0.5f, 0.5f, 1.0f};
        glUniform4fv(4, 1, glm::value_ptr(col));
        glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(*(editor.cam.getView())));
        glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(*(editor.cam.getProj())));
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
            glm::value_ptr(
                glm::translate(glm::vec3(-1.f, -1.f, -1.f)) * glm::scale(glm::vec3(2.f, 2.f, 2.f))));
        // clang-format on
        col = {0.0f, 0.5f, 0.0f, 1.0f};
        glUniform4fv(4, 1, glm::value_ptr(col));
        glBindVertexArray(debugLinesVAO);
        glDrawArrays(GL_LINES, 0, debugLinesPointBufferSize);

        minimalColorShader.UseProgram();
        // glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0)));
        glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(*(editor.cam.getView())));
        glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(*(editor.cam.getProj())));
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
        trackShader.setVec2("minMaxX", featureMinMaxValues[graphingFeature1]);
        trackShader.setVec2("minMaxY", featureMinMaxValues[graphingFeature2]);
        trackShader.setVec2("minMaxZ", featureMinMaxValues[graphingFeature3]);
        glUniformMatrix4fv(0, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glUniformMatrix4fv(1, 1, GL_FALSE, glm::value_ptr(*(editor.cam.getView())));
        glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(*(editor.cam.getProj())));

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
        // todo: put somewhere else, doesnt need static etc (also min/max for all filters etc)
        static bool updateGraphing = false;
        static bool updateFiltering = false;
        static constexpr char* comboNames = TrackData::FeatureNamesData;
        // ImGui::SetNextWindowPos(ImVec2(width - 400, 0), ImGuiCond_Always);
        // ImGui::SetNextWindowSize(ImVec2(350, 800), ImGuiCond_Always);
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
            IMGUI_ACTIVATE(ImGui::Combo("X Axis Value", &graphingFeature1, comboNames), updateGraphing);
            IMGUI_ACTIVATE(ImGui::Combo("Y Axis Value", &graphingFeature2, comboNames), updateGraphing);
            IMGUI_ACTIVATE(ImGui::Combo("Z Axis Value", &graphingFeature3, comboNames), updateGraphing);
            ImGui::Separator();
            ImGui::TextUnformatted("Filter Dataset:");
            ImGui::Text("Track or Artist name");
            if(ImGui::InputText("##filterInput", stringFilterBuffer.data(), stringFilterBuffer.size()))
            {
                updateFiltering = true;
            }
            ImGui::SameLine();
            if(ImGui::Button("X##text"))
            {
                stringFilterBuffer.fill('\0');
                updateFiltering = true;
            }
            for(auto i = 0; i < TrackData::featureAmount; i++)
            {
                float max = i != 7 ? 1.0f : 300.0f;
                float speed = i != 7 ? 0.001f : 1.0f;
                ImGui::TextUnformatted(TrackData::FeatureNames[i].data());
                IMGUI_ACTIVATE(
                    ImGui::DragFloatRange2(
                        ("Min/Max##Feature" + std::to_string(i)).c_str(),
                        &featureMinMaxValues[i].x,
                        &featureMinMaxValues[i].y,
                        speed,
                        0.0f,
                        max),
                    updateFiltering);
                ImGui::SameLine();
                if(ImGui::Button((u8"↺##Feature" + std::to_string(i)).c_str()))
                {
                    featureMinMaxValues[i] = {0.f, max};
                    updateFiltering = true;
                }
            }
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::Separator();
            // todo:
            // probably better to make this a contextual menu when right(or left) clicking a track in the 3d
            // view saves akward mouse movement from center view to the side of the screen
            if(selectedTrack != nullptr)
            {
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                if(ImGui::InvisibleButton("hiddenButtonSelected", ImVec2(64, 64)))
                {
                    apiAccess.startTrackPlayback(selectedTrack->id);
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
                    if(std::find(pinnedTracks.begin(), pinnedTracks.end(), selectedTrack) ==
                       pinnedTracks.end())
                    {
                        pinnedTracks.push_back(selectedTrack);
                    }
                }
            }
        }
        ImGui::End();
        if(updateFiltering)
        {
            std::string_view s(stringFilterBuffer.data());
            std::wstring ws = utf8_decode(s);
            // skip tempo feature
            filteredPlaylistData.clear();
            for(auto& track : playlistData)
            {
                for(auto i = 0; i < TrackData::featureAmount; i++)
                {
                    if(track.features[i] < featureMinMaxValues[i].x ||
                       track.features[i] > featureMinMaxValues[i].y)
                    {
                        goto failedFilter;
                    }
                }
                if(!s.empty())
                {
                    if(track.trackName.find(ws) == std::string::npos &&
                       track.artistsNames.find(ws) == std::string::npos)
                    {
                        goto failedFilter;
                    }
                }
                filteredPlaylistData.push_back(&track);
            failedFilter:;
            }
            // also have to re-sort here;
            if(filteredPlaylistData.size() > 1)
            {
                if(sortAscending)
                {
                    std::sort(filteredPlaylistData.begin(), filteredPlaylistData.end(), sortLambda);
                }
                else
                {
                    std::sort(filteredPlaylistData.rbegin(), filteredPlaylistData.rend(), sortLambda);
                }
            }
            updateGraphing = true;
            updateFiltering = false;
        }
        if(updateGraphing)
        {
            fillTrackBuffer(graphingFeature1, graphingFeature2, graphingFeature3);
            updateGraphing = false;
        }

        if(!editor.uiHidden)
        {
            // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can
            // browse its code to learn more about Dear ImGui!).
            if(show_demo_window)
                ImGui::ShowDemoWindow(&show_demo_window);

            // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named
            // window.
            {
                static float f = 0.0f;
                static int counter = 0;

                ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!" and append into it.

                ImGui::Text(
                    "This is some useful text."); // Display some text (you can use a format strings too)
                ImGui::Checkbox(
                    "Demo Window", &show_demo_window); // Edit bools storing our window open/close state
                ImGui::Checkbox("Another Window", &show_another_window);

                ImGui::SliderFloat("float", &f, 0.0f, 1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
                ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

                if(ImGui::Button("Button")) // Buttons return true when clicked (most widgets return true when
                                            // edited/activated)
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

            // SECTION: Tables
            if(ImGui::Begin("Playlist Data", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                const ImVec2 rowSize(0, coverSize);
                constexpr ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                                  ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable |
                                                  ImGuiTableFlags_NoSavedSettings;
                // SECTION: PINNED TABLE
                ImGui::Text("Pinned Tracks: %d", static_cast<int>(pinnedTracks.size()));
                const ImVec2 outer_size_pin =
                    ImVec2(0, rowSize.y * (std::min<int>(pinnedTracks.size(), 3) + 0.7f));
                if(ImGui::BeginTable("Pinned Tracks", 14, flags, outer_size_pin))
                {
                    ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                    for(const auto& header : columnHeaders)
                    {
                        ImGui::TableSetupColumn(header.name.c_str(), header.flags, header.width);
                    }
                    ImGui::TableSetupColumn("##unpin", noSortColumnFlag, 50.f);
                    ImGui::TableHeadersRow();

                    // Sort our data if sort specs have been changed!
                    // todo: handle sorting in pinned tracks (if at all)
                    if(ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
                    {
                        if(sorts_specs->SpecsDirty)
                        {
                            const auto& sortSpecObj = sorts_specs->Specs[0];
                            // need to write columnToSortBy for lambda to work, so store old value used in
                            // actual table
                            const int oldColumnToSortBy = columnToSortBy;
                            columnToSortBy = sortSpecObj.ColumnIndex;
                            if(pinnedTracks.size() > 1)
                            {
                                if(sortSpecObj.SortDirection == ImGuiSortDirection_Ascending)
                                {
                                    std::sort(pinnedTracks.begin(), pinnedTracks.end(), sortLambda);
                                }
                                else
                                {
                                    std::sort(pinnedTracks.rbegin(), pinnedTracks.rend(), sortLambda);
                                }
                            }
                            columnToSortBy = oldColumnToSortBy;
                            sorts_specs->SpecsDirty = false;
                        }
                    }

                    // Demonstrate using clipper for large vertical lists
                    ImGuiListClipper clipper;
                    clipper.Begin(pinnedTracks.size());
                    int unpinAfterFrame = -1;
                    while(clipper.Step())
                    {
                        for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                        {
                            // todo:
                            // store index of last played track instead, then highlighting can work easily in
                            // multiple tables
                            //  if(row - 1 == lastPlayedTrack)
                            //  {
                            //      ImGui::TableSetBgColor(
                            //          ImGuiTableBgTarget_RowBg0, IM_COL32(122, 122, 122, 80));
                            //  }
                            ImGui::TableNextRow();
                            ImGui::PushID(row);

                            float rowTextOffset = (rowSize.y - ImGui::GetTextLineHeightWithSpacing()) * 0.5f;

                            ImGui::TableSetColumnIndex(0);
                            ImVec2 availReg = ImGui::GetContentRegionAvail();
                            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                            if(ImGui::InvisibleButton(
                                   "hiddenPlayButton##pinned", ImVec2(availReg.x, rowSize.y)))
                            {
                                apiAccess.startTrackPlayback(pinnedTracks[row]->id);
                                // todo: see above
                                // lastPlayedTrack = row;
                            }
                            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + rowTextOffset));
                            if(ImGui::IsItemHovered())
                            {
                                ImGui::Text(u8"▶");
                            }
                            else
                            {
                                ImGui::Text("%d", pinnedTracks[row]->index + 1);
                            }
                            // ImGui::SetCursorScreenPos(cursorPos);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::Image(
                                (void*)(intptr_t)(pinnedTracks[row]->coverInfoPtr->id),
                                ImVec2(coverSize, coverSize));

                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%s", pinnedTracks[row]->trackNameEncoded.c_str());
                            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
                            ImGui::Text("%s", pinnedTracks[row]->albumNameEncoded.c_str());
                            ImGui::PopStyleColor();

                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%s", pinnedTracks[row]->artistsNamesEncoded.c_str());

                            for(int i = 4; i < 11; i++)
                            {
                                ImGui::TableSetColumnIndex(i);
                                ImGui::Text("%.3f", pinnedTracks[row]->features[i - 4]);
                            }

                            ImGui::TableSetColumnIndex(11);
                            ImGui::Text("%.0f", std::round(pinnedTracks[row]->features[7]));

                            ImGui::TableSetColumnIndex(12);
                            ImGui::Text("%.3f", pinnedTracks[row]->features[8]);

                            ImGui::TableSetColumnIndex(13);
                            if(ImGui::Button("Unpin"))
                            {
                                unpinAfterFrame = row;
                            }

                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTable();
                    if(pinnedTracks.size() > 0)
                    {
                        if(ImGui::Button("Create filters from pinned tracks"))
                        {
                            featureMinMaxValues.fill(glm::vec2(
                                std::numeric_limits<float>::max(), std::numeric_limits<float>::min()));
                            for(const auto& trackPtr : pinnedTracks)
                            {
                                for(auto indx = 0; indx < trackPtr->features.size(); indx++)
                                {
                                    featureMinMaxValues[indx].x =
                                        std::min(featureMinMaxValues[indx].x, trackPtr->features[indx]);
                                    featureMinMaxValues[indx].y =
                                        std::max(featureMinMaxValues[indx].y, trackPtr->features[indx]);
                                }
                            }
                            updateFiltering = true;
                        }
                    }
                    if(unpinAfterFrame != -1)
                    {
                        pinnedTracks.erase(pinnedTracks.begin() + unpinAfterFrame);
                        unpinAfterFrame = -1;
                    }
                }
                ImGui::Separator();
                // SECTION: FILTER TABLE
                ImGui::Text("Total: %d", static_cast<int>(filteredPlaylistData.size()));
                const ImVec2 outer_size = ImVec2(0, rowSize.y * 14);
                if(ImGui::BeginTable("Playlist Tracks", 14, flags, outer_size))
                {
                    ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                    for(const auto& header : columnHeaders)
                    {
                        ImGui::TableSetupColumn(header.name.c_str(), header.flags, header.width);
                    }
                    ImGui::TableSetupColumn("##pin", noSortColumnFlag, 50.f);
                    ImGui::TableHeadersRow();

                    // Sort our data if sort specs have been changed!
                    if(ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
                    {
                        if(sorts_specs->SpecsDirty)
                        {
                            const auto& sortSpecObj = sorts_specs->Specs[0];
                            columnToSortBy = sortSpecObj.ColumnIndex;
                            sortAscending = (sortSpecObj.SortDirection == ImGuiSortDirection_Ascending);
                            if(filteredPlaylistData.size() > 1)
                            {
                                if(sortAscending)
                                {
                                    std::sort(
                                        filteredPlaylistData.begin(), filteredPlaylistData.end(), sortLambda);
                                }
                                else
                                {
                                    std::sort(
                                        filteredPlaylistData.rbegin(),
                                        filteredPlaylistData.rend(),
                                        sortLambda);
                                }
                            }
                            sorts_specs->SpecsDirty = false;
                        }
                    }

                    // Demonstrate using clipper for large vertical lists
                    ImGuiListClipper clipper;
                    clipper.Begin(filteredPlaylistData.size());
                    while(clipper.Step())
                    {
                        for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                        {
                            ImGui::TableNextRow();
                            ImGui::PushID(row);

                            if(row == lastPlayedTrack)
                            {
                                ImGui::TableSetBgColor(
                                    ImGuiTableBgTarget_RowBg0, IM_COL32(122, 122, 122, 80));
                            }

                            float rowTextOffset = (rowSize.y - ImGui::GetTextLineHeightWithSpacing()) * 0.5f;

                            ImGui::TableSetColumnIndex(0);

                            ImVec2 availReg = ImGui::GetContentRegionAvail();
                            ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                            if(ImGui::InvisibleButton("hiddenPlayButton", ImVec2(availReg.x, rowSize.y)))
                            {
                                apiAccess.startTrackPlayback(filteredPlaylistData[row]->id);
                                lastPlayedTrack = row;
                            }
                            ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + rowTextOffset));
                            if(ImGui::IsItemHovered())
                            {
                                ImGui::Text(u8"▶");
                            }
                            else
                            {
                                ImGui::Text("%d", filteredPlaylistData[row]->index + 1);
                            }
                            // ImGui::SetCursorScreenPos(cursorPos);

                            ImGui::TableSetColumnIndex(1);
                            ImGui::Image(
                                (void*)(intptr_t)(filteredPlaylistData[row]->coverInfoPtr->id),
                                ImVec2(coverSize, coverSize));

                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%s", filteredPlaylistData[row]->trackNameEncoded.c_str());
                            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
                            ImGui::Text("%s", filteredPlaylistData[row]->albumNameEncoded.c_str());
                            ImGui::PopStyleColor();

                            ImGui::TableSetColumnIndex(3);
                            ImGui::Text("%s", filteredPlaylistData[row]->artistsNamesEncoded.c_str());

                            for(int i = 4; i < 11; i++)
                            {
                                ImGui::TableSetColumnIndex(i);
                                ImGui::Text("%.3f", filteredPlaylistData[row]->features[i - 4]);
                            }

                            ImGui::TableSetColumnIndex(11);
                            ImGui::Text("%.0f", std::round(filteredPlaylistData[row]->features[7]));

                            ImGui::TableSetColumnIndex(12);
                            ImGui::Text("%.3f", filteredPlaylistData[row]->features[8]);

                            ImGui::TableSetColumnIndex(13);
                            if(ImGui::Button("Pin"))
                            {
                                if(std::find(
                                       pinnedTracks.begin(), pinnedTracks.end(), filteredPlaylistData[row]) ==
                                   pinnedTracks.end())
                                {
                                    pinnedTracks.push_back(filteredPlaylistData[row]);
                                    lastPlayedTrack = row;
                                }
                            }

                            ImGui::PopID();
                        }
                    }
                    ImGui::EndTable();
                }
                // ImGui::Text("Size: %f x %f", ImGui::GetItemRectSize().x, ImGui::GetItemRectSize().y);
                static bool canLoadCovers = true;
                if(canLoadCovers)
                {
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
                        for(std::pair<const std::string, CoverInfo>& entry : coverTable)
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
                if(ImGui::Button("Stop Playback"))
                {
                    apiAccess.stopPlayback();
                }
                ImGui::SameLine();
                if(ImGui::Button("Export filtered data to playlist"))
                {
                    // todo: promt popup to ask for PL name
                    std::vector<std::string> uris = {};
                    uris.reserve(filteredPlaylistData.size());
                    for(TrackData* track : filteredPlaylistData)
                    {
                        uris.emplace_back("spotify:track:" + track->id);
                    }
                    apiAccess.createPlaylist(uris);
                }
            }
            ImGui::End();
        }
        // Create ImGui Render Data
        ImGui::Render();

        // Draw the Render Data into framebuffer
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffer
        glfwSwapBuffers(window);
    }
    glDeleteTextures(1, &coverArrayHandle);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
