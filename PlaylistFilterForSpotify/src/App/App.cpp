#include "App.h"
#include "GLFW/glfw3.h"
#include "Renderer/Renderer.h"

App::App()
    : renderer(*this), pinnedTracksTable(*this, pinnedTracks), filteredTracksTable(*this, filteredTracks)
{
    // Spotify Api //////////////////////////////////////////////////
    apiAccess = SpotifyApiAccess();
    const std::string playlist_test_id = "4yDYkPpEix7s5HK5ZBd7lz"; // art pop
    // const std::string playlist_test_id = "0xmNlq3D0z3Dxkt0T0mqyj"; // liked
    // have to use std::tie for now since CLANG doesnt allow for structured bindings to be captured in lambda
    // switch back if lambda refactored into function
    std::tie(playlist, coverTable) = apiAccess.buildPlaylistData(playlist_test_id);
    // auto [playlist, coverTable] = apiAccess.buildPlaylistData(playlist_test_id);

    featureMinMaxValues.fill(glm::vec2(0.0f, 1.0f));
    featureMinMaxValues[7] = {0, 300};

    playlistTracks = std::vector<Track*>(playlist.size());
    for(auto i = 0; i < playlist.size(); i++)
    {
        playlistTracks[i] = &playlist[i];
    }
    // initially the filtered playlist is the same as the original
    filteredTracks = playlistTracks;

    renderer.init();
    pinnedTracksTable.calcHeaderWidth();
    filteredTracksTable.calcHeaderWidth();
}

App::~App()
{
}

bool App::shouldClose()
{
    return glfwWindowShouldClose(renderer.window);
}

void App::run()
{
    while(!shouldClose())
    {
        renderer.draw();
        // todo: factor out into refreshFilter() method?
        if(filterDirty)
        {
            std::string_view s(stringFilterBuffer.data());
            std::wstring ws = utf8_decode(s);
            filteredTracks.clear();
            for(auto& track : playlist)
            {
                for(auto i = 0; i < Track::featureAmount; i++)
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
                filteredTracks.push_back(&track);
            failedFilter:;
            }
            // also have to re-sort here;
            filteredTracksTable.sortData();
            graphingDirty = true;
            filterDirty = false;
        }
        if(graphingDirty)
        {
            renderer.rebuildBuffer();
            graphingDirty = false;
        }
    }
}

bool App::pinTrack(Track* track)
{
    if(std::find(pinnedTracks.begin(), pinnedTracks.end(), track) == pinnedTracks.end())
    {
        pinnedTracks.push_back(track);
        return true;
    }
    return false;
}

bool App::startTrackPlayback(const std::string& trackId)
{
    apiAccess.startTrackPlayback(trackId);
    // todo: handle request return codes
    return true;
}

bool App::stopPlayback()
{
    apiAccess.stopPlayback();
}

void App::createPlaylist(const std::vector<Track*>& tracks)
{
    std::vector<std::string> uris = {};
    uris.reserve(tracks.size());
    for(const auto& track : tracks)
    {
        uris.emplace_back("spotify:track:" + track->id);
    }

    const int MAXLEN = 80;
    char s[MAXLEN] = "PlaylistFilter generated playlist - ";
    time_t t = time(0);
    strftime(&s[36], MAXLEN, "%d/%m/%Y::%H:%M", localtime(&t));

    apiAccess.createPlaylist(&s[0], uris);
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    Renderer* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if((button == GLFW_MOUSE_BUTTON_MIDDLE || button == GLFW_MOUSE_BUTTON_RIGHT) && action == GLFW_PRESS)
    {
        glfwGetCursorPos(window, &renderer->mouse_x, &renderer->mouse_y);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        renderer->cam.setMode(CAMERA_FLY);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        renderer->cam.setMode(CAMERA_ORBIT);
    }
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
            return;
        double mx = 0;
        double my = 0;
        glfwGetCursorPos(window, &mx, &my);
        // TODO: put in func
        glm::mat4 invProj = glm::inverse(*(renderer->cam.getProj()));
        glm::mat4 invView = glm::inverse(*(renderer->cam.getView()));
        float screenX = 2.f * (static_cast<float>(mx) / renderer->width) - 1.f;
        float screenY = -2.f * (static_cast<float>(my) / renderer->height) + 1.f;
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
            renderer->app.featureMinMaxValues[renderer->graphingFeature1].x,
            renderer->app.featureMinMaxValues[renderer->graphingFeature2].x,
            renderer->app.featureMinMaxValues[renderer->graphingFeature3].x};
        glm::vec3 axisMaxs{
            renderer->app.featureMinMaxValues[renderer->graphingFeature1].y,
            renderer->app.featureMinMaxValues[renderer->graphingFeature2].y,
            renderer->app.featureMinMaxValues[renderer->graphingFeature3].y};
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

        for(const auto& trackBufferElement : renderer->trackBuffer)
        {
            glm::vec3 tboP = trackBufferElement.p;
            tboP = (tboP - axisMins) / axisFactors;
            float t = glm::dot(tboP - rayStart, n) / glm::dot(rayDir, n);
            t = std::max(0.f, t);
            hitP = rayStart + t * rayDir;

            float localX = glm::dot(hitP - tboP, worldCamX);
            float localY = glm::dot(hitP - tboP, worldCamY);
            bool insideSquare = std::abs(localX) < 0.5f * renderer->coverSize3D &&
                                std::abs(localY) < 0.5f * renderer->coverSize3D;
            if(insideSquare && t < hit.t)
            {
                hit.t = t;
                hit.index = trackBufferElement.originalIndex;
                resP = tboP;
                debugHitP = hitP;
            }
        }
        renderer->selectedTrack = nullptr;
        if(hit.index != std::numeric_limits<uint32_t>::max())
        {
            renderer->selectedTrack = &(renderer->app.playlist)[hit.index];

            // TODO: ifdef this out, when debugging selection isnt needed
            std::vector<glm::vec3> newData = {worldN, worldF, debugHitP, resP};
            glNamedBufferData(
                renderer->debugLinesPointBuffer,
                sizeof(glm::vec3) * newData.size(),
                newData.data(),
                GL_STATIC_DRAW);
            renderer->debugLinesPointBufferSize = newData.size();
        }
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Renderer* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if(key == GLFW_KEY_TAB && action == GLFW_PRESS)
        renderer->uiHidden = !renderer->uiHidden;
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    Renderer* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        if(renderer->cam.mode == CAMERA_ORBIT)
            renderer->cam.changeRadius(yoffset < 0);
        else if(renderer->cam.mode == CAMERA_FLY)
        {
            float factor = (yoffset > 0) ? 1.1f : 1 / 1.1f;
            renderer->cam.flySpeed *= factor;
        }
    }
}

void resizeCallback(GLFWwindow* window, int w, int h)
{
    Renderer* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->width = w;
    renderer->height = h;
    renderer->cam.setAspect(static_cast<float>(w) / static_cast<float>(h));
    glViewport(0, 0, w, h);
}