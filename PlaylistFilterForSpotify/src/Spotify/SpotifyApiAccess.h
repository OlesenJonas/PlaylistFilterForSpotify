#pragma once

#include <iostream>
#include <optional>
#include <string>

#include <cpr/cpr.h>
#include <json.hpp>

#include "ApiError.h"
#include "CommonStructs/CommonStructs.h"
#include "Track/Track.h"
#include "secrets.h"
#include "utils/utf.h"

using nlohmann::json;

class SpotifyApiAccess
{
  public:
    SpotifyApiAccess();

    std::string getAuthURL();
    // have to pass as std::string :/ CPR Constructor takes only string not _view
    bool checkAuth(const std::string& p_state, const std::string& code);
    void refreshAccessToken();

    std::tuple<std::vector<Track>, std::unordered_map<std::string, CoverInfo>>
    buildPlaylistData(std::string_view playlistID, float* progressTracker);

    json getAlbum(const std::string& albumId);

    std::optional<std::string> checkPlaylistExistance(std::string_view id);

    // todo: not void, handle errors (especially if no device found because inactive!)

    void startTrackPlayback(const std::string& trackUris);
    void stopPlayback();
    void createPlaylist(std::string_view name, const std::vector<std::string>& trackUris);

    // Get the Ids of track recommendations based on up to 5 input track Ids
    std::vector<std::string> getRecommendations(std::vector<std::string_view>& seedIds);

  private:
    std::string state;
    std::string code_verifier;

    std::string userId;
    std::string refresh_token;
    std::string access_token;
};