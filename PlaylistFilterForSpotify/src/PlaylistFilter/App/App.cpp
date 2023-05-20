#include <App/App.hpp>
#include <DynamicBitset/DynamicBitset.hpp>
#include <Renderer/Renderer.hpp>

#include <GLFW/glfw3.h>
#include <chrono>
#include <future>
#include <random>
#include <stb/stb_image.h>
#include <string_view>
#include <unordered_set>

// todo: why? cant remember
#ifdef _WIN32
    #include <shellapi.h>
    #include <windows.h>
#endif

App::App() : renderer(*this), pinnedTracksTable(*this, pinnedTracks), filteredTracksTable(*this, filteredTracks)
{
    apiAccess = SpotifyApiAccess();
    resetFilterValues();
    userInput.fill(0);
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
        switch(state)
        {
        case State::LOG_IN:
            runLogIn();
            break;
        case State::PLAYLIST_SELECT:
            runPlaylistSelect();
            break;
        case State::PLAYLIST_LOAD:
            runPlaylistLoad();
            break;
        case State::MAIN:
            runMain();
            break;
        default:
            assert(false && "App state not handled");
        }
    }
}

void App::runLogIn()
{
    // todo: can this be moved to the end? not 100% sure where(if?) ImGui resetes drawData
    renderer.startFrame();

    createLogInUI();

    renderer.drawBackgroundWindow();
    renderer.drawUI();
    renderer.endFrame();
}
void App::createLogInUI()
{
    ImGui::SetNextWindowSize({60 * ImGui::CalcTextSize("M").x, 0.f});
    ImGui::Begin(
        "LogIn",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    ImVec2 size = ImGui::GetWindowSize();
    ImGui::SetWindowPos(ImVec2(renderer.width / 2.0f - size.x / 2.0f, renderer.height / 2.0f - size.y / 2.0f));
    ImGui::TextWrapped("The following button will redirect you to Spotify in order"
                       "to give this application the necessary permissions. "
                       "Afterwards you will be redirected to another URL. Please "
                       "copy that URL and paste it into the following field.");
    ImGui::Dummy({0, renderer.scaleByDPI(1.0f)});
    if(ImGui::Button("Open Spotify"))
    {
        requestAuth();
    }
    ImGui::Dummy({0, renderer.scaleByDPI(1.0f)});
    ImGui::TextUnformatted("URL:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
    // ImGui::InputText(
    //     "##accessURL",
    //     userInput.data(),
    //     userInput.size(),
    //     ImGuiInputTextFlags_CallbackResize,
    //     ImGui::resizeUserInputVector,
    //     &userInput);
    ImGui::InputText("##accessURL", userInput.data(), userInput.size());
    ImGui::Dummy({0, renderer.scaleByDPI(1.0f)});
    if(ImGui::Button("Log in using URL"))
    {
        if(!checkAuth())
        {
            ImGui::OpenPopup("Login Error");
        }
        else
        {
            state = App::State::PLAYLIST_SELECT;
            std::fill(userInput.begin(), userInput.end(), '\0');
        }
    }
    if(ImGui::BeginPopup("Login Error"))
    {
        ImGui::Text("Error logging in using that URL\nPlease try again");
        ImGui::EndPopup();
    }
    ImGui::End();
}

void App::runPlaylistSelect()
{
    renderer.startFrame();

    createPlaylistSelectUI();

    renderer.drawBackgroundWindow();
    renderer.drawUI();
    renderer.endFrame();
}
void App::createPlaylistSelectUI()
{
    ImGui::Begin(
        "PlaylistSelection",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    ImVec2 size = ImGui::GetWindowSize();
    ImGui::SetWindowPos(ImVec2(renderer.width / 2.0f - size.x / 2.0f, renderer.height / 2.0f - size.y / 2.0f));
    ImGui::TextUnformatted("Enter Playlist URL or ID");
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
    ImGui::TextUnformatted("eg:\n"
                           "- https://open.spotify.com/playlist/37i9dQZF1DX4jP4eebSWR9?si=427d9c1af97c4600\n"
                           "or just:\n"
                           "- 37i9dQZF1DX4jP4eebSWR9");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(60 * ImGui::CalcTextSize("M").x);
    if(ImGui::InputText("##playlistInput", userInput.data(), userInput.size() - 1))
    {
        // todo: explicit "check" button instead of refreshing on each input action
        playlistID.clear();
        extractPlaylistIDFromInput();
        if(!playlistID.empty())
        {
            // check if id is valid id
            playlistName = apiAccess.checkPlaylistExistance(playlistID);
        }
    }
    if(!playlistID.empty())
    {
        if(!playlistName.empty())
        {
            ImGui::Text("Playlist found: %s", playlistName.c_str());
            if(ImGui::Button("Load Playlist##selection"))
            {
                state = App::State::PLAYLIST_LOAD;
                doneLoading = std::async(std::launch::async, &App::loadSelectedPlaylist, this);
            }
        }
        else
        {
            ImGui::Text(
                "No Playlist found with ID: %.*s\nMake sure you have the necessary permissions to access the "
                "playlist",
                static_cast<int>(playlistID.size()),
                playlistID.data());
        }
    }
    else
    {
        ImGui::TextUnformatted("No ID found in input!");
    }
    ImGui::End();
}

void App::runPlaylistLoad()
{
    renderer.startFrame();

    createPlaylistLoadUI();

    renderer.drawBackgroundWindow();
    renderer.drawUI();
    renderer.endFrame();

    std::future_status status = doneLoading.wait_for(std::chrono::seconds(0));
    if(status == std::future_status::ready)
    {
        // can only upload to GPU from main thread, so this last step has to happen here
        // to account for the extra "loading time" the progress bar in renderer.cpp only goes up to 90% :)
        renderer.buildRenderData();
        state = State::MAIN;
    }
}
void App::createPlaylistLoadUI()
{
    ImGui::Begin(
        "##PlaylistLoading",
        nullptr,
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 size = ImGui::GetWindowSize();
        ImGui::SetWindowPos(ImVec2(renderer.width / 2.0f - size.x / 2.0f, renderer.height / 2.0f - size.y / 2.0f));
        const float barWidth = static_cast<float>(renderer.width) / 3.0f;
        const float barHeight = renderer.scaleByDPI(30.0f);
        // ImGui::SetCursorScreenPos({width / 2.0f - barWidth / 2.0f, height / 2.0f});
        ImGui::HorizontalBar(0.0f, loadPlaylistProgress, {barWidth, barHeight});
    }
    ImGui::End();
}

void App::runMain()
{
    renderer.startFrame();

    renderer.uploadAvailableCovers();

    createMainUI();

    renderer.draw3DGraph();
    renderer.drawBackgroundWindow();

    renderer.drawUI();
    renderer.endFrame();

    // Have to do this after creating UI is done, otherwise conflicts happen
    // todo: factor out into refreshFilter() method !!!!
    if(filterDirty)
    {
        filteredTracks.clear();
        for(auto& track : playlist)
        {
            for(auto i = 0; i < Track::featureAmount; i++)
            {
                if(track.features[i] < featureMinMaxValues[i].x || track.features[i] > featureMinMaxValues[i].y)
                {
                    goto failedFilter;
                }
            }
            if(currentGenreMask)
            {
                if(!(currentGenreMask & track.genreMask))
                {
                    goto failedFilter;
                }
            }
            if(nameFilter.InputBuf[0] != 0)
            {
                if(!nameFilter.PassFilter(track.artistsNamesEncoded.c_str()) &&
                   !nameFilter.PassFilter(track.albumNameEncoded.c_str()) &&
                   !nameFilter.PassFilter(track.trackNameEncoded.c_str()))
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
void App::createMainUI()
{
    static constexpr char* comboNames = Track::FeatureNamesData;
    ImGui::Begin(
        "Graphing Settings",
        nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);
    {
        auto size = ImGui::GetWindowSize();
        ImGui::SetWindowPos(ImVec2(renderer.width - size.x, 0));

        ImGui::SliderFloat("Cover Size", &coverSize3D, 0.0f, 0.1f);
        ImGui::Separator();
        IMGUI_ACTIVATE(ImGui::Combo("X Axis Value", &graphingFeatureX, comboNames), graphingDirty);
        IMGUI_ACTIVATE(ImGui::Combo("Y Axis Value", &graphingFeatureY, comboNames), graphingDirty);
        IMGUI_ACTIVATE(ImGui::Combo("Z Axis Value", &graphingFeatureZ, comboNames), graphingDirty);
        ImGui::Separator();

        ImGui::TextUnformatted("Filter Playlist:");

        ImGui::TextUnformatted("Artist \"genres\"");
        ImGui::SameLine();
        ImGui::HelpMarker("Spotify does not offer genres per track. So this will only use the "
                          "genres assigned to any of the track's artists. As a result the filtering "
                          "may be less accurate.");
        genreFilter.Draw("Search genres");
        ImGui::SameLine();
        if(ImGui::Button("X##genreFilter"))
        {
            genreFilter.Clear();
        }
        ImGui::HelpMarkerFromLastItem("Reset filter");
        if(ImGui::BeginChild(
               "##genreSelectionChild",
               ImVec2(size.x - 2.0f * ImGui::GetStyle().WindowPadding.x, renderer.scaleByDPI(125.0f)),
               true))
        {
            for(uint32_t i = 0; i < genreNames.size(); i++)
            {
                if(genreFilter.PassFilter(genreNames[i].c_str()))
                {
                    const bool isSelected = currentGenreMask.getBit(i);
                    if(ImGui::Selectable(genreNames[i].c_str(), isSelected))
                    {
                        currentGenreMask.toggleBit(i);
                        filterDirty = true;
                    }
                }
            }
        }
        ImGui::EndChild();
        if(ImGui::Button("↺##genreFilter"))
        {
            currentGenreMask.clear();
            filterDirty = true;
        }

        ImGui::Text("Track, Artist, Album name");
        if(nameFilter.Draw("##"))
        {
            filterDirty = true;
        }
        ImGui::SameLine();
        if(ImGui::Button("↺##text"))
        {
            nameFilter.Clear();
            filterDirty = true;
        }
        for(auto i = 0; i < Track::featureAmount; i++)
        {
            ImGui::PushID(i);
            float max = i != 7 ? 1.0f : 300.0f;
            float speed = i != 7 ? 0.001f : 1.0f;
            ImGui::TextUnformatted(Track::FeatureNames[i].data());
            IMGUI_ACTIVATE(
                ImGui::DragFloatRange2(
                    "Min/Max", &featureMinMaxValues[i].x, &featureMinMaxValues[i].y, speed, 0.0f, max),
                filterDirty);
            ImGui::SameLine();
            if(ImGui::Button("↺"))
            {
                featureMinMaxValues[i] = {0.f, max};
                filterDirty = true;
            }
            ImGui::HelpMarkerFromLastItem("Reset filter");
            ImGui::PopID();
        }
        ImGui::Dummy(ImVec2(0.0f, 1.0f));
        if(ImGui::Button("Reset all ↺"))
        {
            resetFilterValues();
            filterDirty = true;
        }
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        ImGui::Separator();
        if(selectedTrack != nullptr)
        {
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            const float coverSize = renderer.scaleByDPI(64.0f);
            if(ImGui::ImageHoverButton(
                   "hiddenButtonSelected",
                   reinterpret_cast<ImTextureID>(selectedTrack->coverInfoPtr->id),
                   reinterpret_cast<ImTextureID>(renderer.spotifyIconHandle),
                   coverSize,
                   0.5f))
            {
                startTrackPlayback(selectedTrack->id);
            }
            float maxTextSize = ImGui::CalcTextSize("MMMMMMMMMMMMMMMMMMMMMMM").x;
            // ImGui::SetCursorPos(textStartPos);
            ImGui::SameLine();
            if(ImGui::BeginChild("Names##GraphingSettings", ImVec2(maxTextSize, coverSize)))
            {
                ImGui::TextUnformatted(selectedTrack->trackNameEncoded.c_str());
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
                ImGui::TextUnformatted(selectedTrack->albumNameEncoded.c_str());
                ImGui::PopStyleColor();
                ImGui::TextUnformatted(selectedTrack->artistsNamesEncoded.c_str());
            }
            ImGui::EndChild();

            // ImGui::SetCursorPos(afterImagePos);
            ImGui::SetNextItemWidth(coverSize);
            if(ImGui::Button("Pin Track"))
            {
                pinTrack(selectedTrack);
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

        if(ImGui::Begin(
               "Playlist Data | Tab to toggle window visibility",
               nullptr,
               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
        {
            if(ImGui::Button("Stop Playback"))
            {
                apiAccess.stopPlayback();
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

                        std::lock_guard<std::mutex> lock(renderer.coverLoadQueueMutex);
                        renderer.coverLoadQueue.push(tli);
                    };
                    coversLoaded = 0;
                    // todo:
                    // handle partially loaded covers (when loading cover earlier went wrong, or new
                    // tracks were added (not yet included)) probably easiest to have a "isLoaded" flag as
                    // part of CoverInfo.
                    // That would enable starting the download again, and only downloading ones that failed the
                    // first time around
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
            ImGui::Separator();

            pinnedTracksTable.draw();
            if(!pinnedTracks.empty())
            {
                if(ImGui::Button("Export to playlist##pins"))
                {
                    // todo: promt popup to ask for PL name
                    createPlaylist(pinnedTracks);
                }

                // todo: un-hardcode these offsets (depend on text (-> button/sliders) sizes)
                ImGui::SameLine(pinnedTracksTable.width - renderer.scaleByDPI(952.f));
                if(ImGui::Button("Recommend tracks to pin"))
                {
                    extendPinsByRecommendations();
                }
                ImGui::SameLine(pinnedTracksTable.width - renderer.scaleByDPI(790.f));
                ImGui::SetNextItemWidth(renderer.scaleByDPI(100.0f));
                ImGui::SliderInt("Accuracy", &recommendAccuracy, 1, 5, "");
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

                ImGui::SameLine(pinnedTracksTable.width - renderer.scaleByDPI(189.f));
                if(ImGui::Button("Create filters from pinned tracks"))
                {
                    // todo: XYZ(vector<Track*> v) that fills filter
                    featureMinMaxValues.fill(
                        glm::vec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::min()));
                    for(const Track* trackPtr : pinnedTracks)
                    {
                        for(auto indx = 0; indx < trackPtr->features.size(); indx++)
                        {
                            featureMinMaxValues[indx].x =
                                std::min(featureMinMaxValues[indx].x, trackPtr->features[indx]);
                            featureMinMaxValues[indx].y =
                                std::max(featureMinMaxValues[indx].y, trackPtr->features[indx]);
                        }
                    }
                    filterDirty = true;
                }
            }
            ImGui::Separator();

            filteredTracksTable.draw();
            if(ImGui::Button("Export to playlist"))
            {
                // todo: promt popup to ask for PL name
                createPlaylist(filteredTracks);
            }
            ImGui::SameLine(filteredTracksTable.width - renderer.scaleByDPI(80.f));
            if(ImGui::Button("Pin all"))
            {
                pinTracks(filteredTracks);
            }
        }
        ImGui::End(); // Playlist Data Window

        if(showRecommendations)
        {
            ImGui::SetNextWindowSize(ImVec2(0, 0));
            ImGui::SetNextWindowSizeConstraints(
                ImVec2(-1, 0), ImVec2(-1, renderer.scaleByDPI(500.0f))); // Vertical only
            if(ImGui::Begin("Pin Recommendations", &showRecommendations, ImGuiWindowFlags_NoSavedSettings))
            {
                if(recommendedTracks.empty())
                {
                    ImGui::TextUnformatted("No recommendations found :(");
                }
                int id = -1;
                for(const auto& recommendation : recommendedTracks)
                {
                    const float coverSize = renderer.scaleByDPI(64.0f);
                    const auto& track = recommendation.track;
                    id++;
                    ImGui::PushID(id);
                    if(ImGui::ImageHoverButton(
                           "hiddenButtonRecommended",
                           reinterpret_cast<ImTextureID>(track->coverInfoPtr->id),
                           reinterpret_cast<ImTextureID>(renderer.spotifyIconHandle),
                           coverSize,
                           0.5f))
                    {
                        startTrackPlayback(track->id);
                    }
                    float maxTextSize = ImGui::CalcTextSize("MMMMMMMMMMMMMMMMMMMMMMMMM").x;
                    ImGui::SameLine();
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
                        pinTrack(track);
                    }
                    ImGui::PopID();
                }
            }
            ImGui::End(); // Pin Recommendations Window
        }
    } // ui hidden

    if(showDeviceErrorWindow)
    {
        ImGui::SetNextWindowFocus();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 255));
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, IM_COL32(180, 50, 50, 255));
        ImGui::Begin(
            "Device Error",
            &showDeviceErrorWindow,
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
        ImGui::PopStyleColor();
        ImGui::TextUnformatted("Warning:\n"
                               "There's currently no active Spotify session!\n"
                               "An inactive session can be refreshed by\n"
                               "eg.: playing & pausing a track.\n");
        ImGui::Separator();
        ImGui::TextUnformatted("The Spotify desktop client can be downloaded for free from:");
        if(ImGui::Button("spotify.com/download"))
        {
#ifdef _WIN32
            ShellExecute(nullptr, nullptr, "https://spotify.com/download", nullptr, nullptr, SW_SHOW);
#else
    #error open (default) webbrowser with given URL
#endif
        }
        ImGui::PopStyleColor();
        ImGui::End();
    }
}

void App::requestAuth()
{
    std::string url = apiAccess.getAuthURL();
#ifdef _WIN32
    ShellExecute(nullptr, nullptr, url.c_str(), nullptr, nullptr, SW_SHOW);
#else
    #error open (default) webbrowser with given URL
#endif
}

bool App::checkAuth()
{
    const std::string_view input{&userInput[0]};
    if(input.find("?error") != std::string_view::npos)
    {
        return false;
    }

    auto statePos = input.find("&state=");
    if(statePos == std::string_view::npos)
    {
        return false;
    }
    // state is last parameter in url, dont need size param. Just use rest of string til \0
    std::string_view state{&input[statePos + 7]};

    auto codePos = input.find("?code=");
    if(codePos == std::string_view::npos)
    {
        return false;
    }
    std::string_view code{&input[codePos + 6], statePos - (codePos + 6)};

    return apiAccess.checkAuth(std::string{state}, std::string{code});
}

void App::loadSelectedPlaylist()
{
    // have to use std::tie for now since CLANG doesnt allow for structured bindings to be captured in
    // lambda can switch back if lambda refactored into function
    std::tie(playlist, coverTable, genreNames) = apiAccess.buildPlaylistData(playlistID, &loadPlaylistProgress);
    // auto [playlist, coverTable] = apiAccess.buildPlaylistData(playlistID);

    currentGenreMask = DynBitset(genreNames.size());
    currentGenreMask.clear();

    playlistTracks = std::vector<Track*>(playlist.size());
    for(auto i = 0; i < playlist.size(); i++)
    {
        playlistTracks[i] = &playlist[i];
    }
    // initially the filtered playlist is the same as the original
    filteredTracks = playlistTracks;
}

void App::extractPlaylistIDFromInput()
{
    std::string_view input = &userInput[0];
    if(input.size() == 22)
    {
        playlistID = input;
    }
    else
    {
        auto res = input.find("/playlist/");
        if(res != std::string_view::npos)
        {
            playlistID = {&userInput[res + 10], 22};
        }
        else
        {
            playlistID = "";
        }
    }
}

std::optional<std::string> App::checkPlaylistID(std::string_view id)
{
    return apiAccess.checkPlaylistExistance(id);
}

void App::resetFilterValues()
{
    featureMinMaxValues.fill(glm::vec2(0.0f, 1.0f));
    featureMinMaxValues[7] = {0, 300};
    nameFilter.Clear();
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

void App::pinTracks(const std::vector<Track*>& tracks)
{
    for(Track* track : tracks)
    {
        if(std::find(pinnedTracks.begin(), pinnedTracks.end(), track) == pinnedTracks.end())
        {
            pinnedTracks.push_back(track);
        }
    }
}

bool App::startTrackPlayback(const std::string& trackId)
{
    bool ret = apiAccess.startTrackPlayback(trackId);
    if(!ret)
    {
        showDeviceErrorWindow = true;
    }
    return ret;
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

void App::extendPinsByRecommendations()
{
    // only recommend tracks that are part of the users playlist
    std::unordered_map<std::string_view, Track*> playlistEntries;
    for(Track& track : playlist)
    {
        playlistEntries.try_emplace(track.id, &track);
    }

    // since we can only request recommendations for up to 5 tracks at once
    // we need to first build a list of requests that covers all pinned tracks
    // the amount of tracks included in one request depends on the selected accuracy
    const int requestSize = recommendAccuracy;
    std::vector<std::vector<std::string_view>> requests;

    // fill requests
    if(requestSize == 1)
    {
        for(const Track* track : pinnedTracks)
        {
            requests.emplace_back(1);
            auto& req = requests.back();
            req[0] = track->id;
        }
    }
    else
    {
        if(pinnedTracks.size() <= requestSize)
        {
            requests.emplace_back(pinnedTracks.size());
            auto& req = requests.back();
            for(int i = 0; i < pinnedTracks.size(); i++)
            {
                req[i] = pinnedTracks[i]->id;
            }
        }
        else
        {
            // this is the more complicated part
            // want to generate random subsets covering all pinned tracks
            // some tracks may occur more often, but the goal is for each
            // track so occur roughly the same amount of times
            // in order to realize that the probably of selecting a track
            // is set to the inverse of how often it has already been selected
            std::random_device rd;
            std::mt19937 rd_gen(rd());

            std::vector<int> occurances(pinnedTracks.size());
            std::fill(occurances.begin(), occurances.end(), 0);
            int totalOccurances = 0;
            std::vector<int> weights(pinnedTracks.size());
            std::fill(weights.begin(), weights.end(), 1);

            for(int i = 0; i < pinnedTracks.size(); i++)
            {
                if(occurances[i] > 0)
                {
                    continue;
                }

                requests.emplace_back(requestSize);
                auto& request = requests.back();

                request[0] = pinnedTracks[i]->id;

                // unless its the first iteration, calculate the weights from the "inverse occurance"
                if(i != 0)
                {
                    for(int j = 0; j < weights.size(); j++)
                    {
                        weights[j] = totalOccurances - occurances[j];
                    }
                }
                std::discrete_distribution<> distrib(weights.begin(), weights.end());

                // fill request with other requestSize-1 elements
                for(int j = 0; j < (requestSize - 1); j++)
                {
                    std::string_view newElem;
                    int indx = 0;
                    do
                    {
                        indx = distrib(rd_gen);
                        newElem = pinnedTracks[indx]->id;
                    }
                    // request should not contain duplicates
                    while(std::find(request.begin(), request.end(), newElem) != request.end());
                    request[j + 1] = newElem;
                    occurances[indx] += 1;
                }

                totalOccurances += requestSize;
            }
        }
    }

    // requests are built, now retrieve recommendations from API and check against playlist

    std::unordered_set<Recommendation, RecommendationHash> tracksToRecommend;

    for(auto& request : requests)
    {
        std::vector<std::string> recommendations = apiAccess.getRecommendations(request);
        for(auto& id : recommendations)
        {
            // better to compare more than just ID, mb. name?
            //(for cases where its the "same" track but in different versions / from diff albums)
            auto found = playlistEntries.find(id);
            if((found != playlistEntries.end()) &&
               (std::find(pinnedTracks.begin(), pinnedTracks.end(), found->second) == pinnedTracks.end()))
            {
                // playlist does contain track

                auto insertion = tracksToRecommend.insert({.track = found->second});
                // increase occurance counter if no insertion happended
                if(!insertion.second)
                {
                    insertion.first->occurances += 1;
                }
            }
        }
    }

    recommendedTracks.assign(tracksToRecommend.begin(), tracksToRecommend.end());
    // note sort with reverse iterators, high occurances should be at front of vector
    std::sort(recommendedTracks.rbegin(), recommendedTracks.rend());
    showRecommendations = true;
    renderer.highlightWindow("Pin Recommendations");
};

Renderer& App::getRenderer()
{
    return renderer;
};