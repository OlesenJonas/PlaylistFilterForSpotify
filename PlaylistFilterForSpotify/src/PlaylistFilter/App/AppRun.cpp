#include "App.hpp"
#include "ImGui/imgui.h"
#include <stb/stb_image.h>

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
            apiAccess.startRefreshThread();
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
        //                                                                                     todo: just no
        graphingData.reserve(playlist.size());
        generateGraphingData();
        renderer.createRenderData();
        renderer.uploadGraphingData(graphingData);
        state = State::MAIN;
    }
}
void App::createPlaylistLoadUI()
{
    ImGui::Begin(
        "##PlaylistLoading",
        nullptr,
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
    {
        ImVec2 size = ImGui::GetWindowSize();
        ImGui::SetWindowPos(ImVec2(renderer.width / 2.0f - size.x / 2.0f, renderer.height / 2.0f - size.y / 2.0f));
        const float barWidth = static_cast<float>(renderer.width) / 3.0f;
        const float barHeight = renderer.scaleByDPI(30.0f);
        // ImGui::SetCursorScreenPos({width / 2.0f - barWidth / 2.0f, height / 2.0f});
        ImGui::Text("%s:", loadingPlaylistProgressLabel.c_str());
        ImGui::HorizontalBar(0.0f, loadPlaylistProgress, {barWidth, barHeight});
    }
    ImGui::End();
}

void App::runMain()
{
    renderer.startFrame();

    if(renderer.uploadAvailableCovers(coversLoaded))
    {
        // also need to regenerate TrackBuffer, so that new layer indices are uploaded to GPU aswell
        //  todo: dont need to replace full buffer, just need to update texture indices!
        generateGraphingData();
        renderer.uploadGraphingData(graphingData);
    }

    createMainUI();

    renderer.draw3DGraph(
        coverSize3D,
        featureMinMaxValues[graphingFeatureX],
        featureMinMaxValues[graphingFeatureY],
        featureMinMaxValues[graphingFeatureZ]);
    renderer.drawBackgroundWindow();

    renderer.drawUI();
    renderer.endFrame();

    // Have to do this after creating UI is done, otherwise conflicts happen
    //      todo: not sure if I want the [flag] = false; lines inside the refresh/generate function
    if(filterDirty)
    {
        refreshFilteredTracks();
        filterDirty = false;
    }
    if(graphingDirty)
    {
        generateGraphingData();
        renderer.uploadGraphingData(graphingData);
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
                    if(!displayOnlySelectedGenres || isSelected)
                    {
                        if(ImGui::Selectable(genreNames[i].c_str(), isSelected))
                        {
                            currentGenreMask.toggleBit(i);
                            filterDirty = true;
                        }
                    }
                }
            }
        }
        ImGui::EndChild();
        if(ImGui::Button("Select all matching genres##genreFilter"))
        {
            for(uint32_t i = 0; i < genreNames.size(); i++)
            {
                if(genreFilter.PassFilter(genreNames[i].c_str()))
                {
                    currentGenreMask.setBit(i);
                    filterDirty = true;
                }
            }
        }
        ImGui::Checkbox("Show only selected##genreFilter", &displayOnlySelectedGenres);
        ImGui::SameLine();
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
            resetFeatureFilters();
            filterDirty = true;
        }
        ImGui::Dummy(ImVec2(0.0f, 5.0f));
        if(selectedTrack != nullptr)
        {
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            const float coverSize = renderer.scaleByDPI(64.0f);
            if(ImGui::ImageHoverButton(
                   "hiddenButtonSelected",
                   reinterpret_cast<ImTextureID>(selectedTrack->coverInfoPtr->id),
                   reinterpret_cast<ImTextureID>(renderer.spotifyIconHandle),
                   coverSize,
                   0.5f))
            {
                startTrackPlayback(selectedTrack);
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
            // if(ImGui::Button("Stop Playback"))
            // {
            //     apiAccess.stopPlayback();
            // }
            if(canLoadCovers)
            {
                // ImGui::SameLine();
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
            if(!canLoadCovers && coversLoaded != coversTotal)
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
                float buttonHeight = ImGui::GetItemRectSize().y;

                static float recommendationsWidth = renderer.scaleByDPI(1000.f);
                float regionAvail = ImGui::GetContentRegionAvailWidth();
                ImGui::SameLine((ImGui::GetContentRegionAvailWidth() - recommendationsWidth) / 2.0f);
                // ImGui::SameLine();
                ImGui::BeginGroup();
                {

                    ImGui::Text("Recommend tracks to pin:");
                    ImGui::SameLine();
                    if(ImGui::Button("Based on tracks"))
                    {
                        extendPinsByRecommendations();
                    }
                    ImGui::SameLine();
                    if(ImGui::Button("Based on artists"))
                    {
                        extendPinsByArtists();
                    }
                    ImGui::SameLine();
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
                }
                ImGui::EndGroup();
                recommendationsWidth = ImGui::GetItemRectSize().x;

                static float createFilterWidth = renderer.scaleByDPI(189.f);
                ImGui::SameLine(pinnedTracksTable.width - createFilterWidth);
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
                createFilterWidth = ImGui::GetItemRectSize().x;
            }
            ImGui::Separator();

            filteredTracksTable.draw();
            if(ImGui::Button("Export to playlist"))
            {
                // todo: promt popup to ask for PL name
                createPlaylist(filteredTracks);
            }
            static float pinAllSize = renderer.scaleByDPI(45.f);
            // ImGui::SameLine(filteredTracksTable.width - pinAllSize);
            ImGui::SameLine(ImGui::GetWindowContentRegionWidth() - pinAllSize);
            if(ImGui::Button("Pin all"))
            {
                pinTracks(filteredTracks);
            }
            pinAllSize = ImGui::GetItemRectSize().x;
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
                        startTrackPlayback(track);
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