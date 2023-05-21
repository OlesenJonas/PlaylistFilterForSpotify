#include "CommonStructs/CommonStructs.hpp"
#include <App/App.hpp>
#include <DynamicBitset/DynamicBitset.hpp>
#include <Renderer/Renderer.hpp>

#include <GLFW/glfw3.h>
#include <chrono>
#include <future>
#include <random>
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

// This is started asynchronously
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

void App::refreshFilteredTracks()
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

void App::generateGraphingData()
{
    graphingData.clear();
    for(const Track* track : filteredTracks)
    {
        // uint32_t index = static_cast<uint32_t>(track - baseptr);
        auto index = std::distance((const Track*)playlist.data(), track);
        graphingData.emplace_back(GraphingBufferElement{
            {track->features[graphingFeatureX],
             track->features[graphingFeatureY],
             track->features[graphingFeatureZ]},
            track->coverInfoPtr->layer,
            (GLuint)index});
    }
}

Renderer& App::getRenderer()
{
    return renderer;
};