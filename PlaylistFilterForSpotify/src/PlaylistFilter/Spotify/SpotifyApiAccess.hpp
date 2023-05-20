#pragma once

#include <iostream>
#include <optional>
#include <string>

#include <cpr/cpr.h>
#include <json/json.hpp>

#include <CommonStructs/CommonStructs.hpp>
#include <Track/Track.hpp>
#include <utils/utf.hpp>

using nlohmann::json;

class SpotifyApiAccess
{
  public:
    SpotifyApiAccess();

    // build URL required to request authorization
    std::string getAuthURL();
    // have to pass as std::string :/ CPR Constructor takes only string not _view
    bool checkAuth(const std::string& p_state, const std::string& code);
    // refresh the users access token
    void refreshAccessToken();

    // todo: handle api errors
    // build the main playlist data, a vector of track objects and a map [Album ID -> CoverInfo Struct]
    // (stores texture handle etc)
    std::tuple<std::vector<Track>, std::unordered_map<std::string, CoverInfo>, std::vector<std::string>>
    buildPlaylistData(std::string_view playlistID, float* progressTracker);
    // get the Album json returned by the api
    json getAlbum(const std::string& albumId);
    /*
        check if a playlist with a given id exists (and is accessible to the user)
        if it is, returns the name of the playlist
        if it is not, returns the empty string
    */
    std::string checkPlaylistExistance(std::string_view id);
    // starts playback of the given song from the users active spotify session
    bool startTrackPlayback(const std::string& trackId);
    // stop the users current playback
    void stopPlayback();
    // create a playlist with given name, consisting of tracks whose uris are stored in the 2nd parameter
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