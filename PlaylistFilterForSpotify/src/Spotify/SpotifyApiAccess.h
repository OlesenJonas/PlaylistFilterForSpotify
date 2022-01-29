#pragma once

#include <iostream>
#include <string>

#include <cpr/cpr.h>
#include <json.hpp>

#include "ApiError.h"
#include "TrackData.h"
#include "secrets.h"
#include "utils/utf.h"

using nlohmann::json;

struct CoverInfo
{
    std::string url;
    GLuint layer = 0;
    GLuint id;
};

class SpotifyApiAccess
{
  public:
    SpotifyApiAccess();

    void refreshAccessToken();

    std::tuple<std::vector<TrackData>, std::unordered_map<std::string, CoverInfo>>
    buildPlaylistData(const std::string& playlistID);
    json getAlbum(const std::string& albumId);

    // todo: not void, handle errors (especially if no device found because inactive!)
    void startTrackPlayback(const std::string& trackUris);
    void stopPlayback();
    void createPlaylist(const std::vector<std::string>& trackIds);

    // is public for now, get/set would be trivial atm
    std::string access_token;
};