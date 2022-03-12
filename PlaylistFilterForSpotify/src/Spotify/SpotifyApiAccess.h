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

    void refreshAccessToken();

    std::tuple<std::vector<Track>, std::unordered_map<std::string, CoverInfo>>
    buildPlaylistData(const std::string& playlistID);

    json getAlbum(const std::string& albumId);

    std::optional<std::string> checkPlaylistExistance(std::string_view id);

    // todo: not void, handle errors (especially if no device found because inactive!)

    void startTrackPlayback(const std::string& trackUris);
    void stopPlayback();
    void createPlaylist(std::string_view name, const std::vector<std::string>& trackUris);

    // Get the Ids of track recommendations based on up to 5 input track Ids
    std::vector<std::string> getRecommendations(std::vector<std::string_view>& seedIds);

    // is public for now, get/set would be trivial atm
    std::string access_token;
};