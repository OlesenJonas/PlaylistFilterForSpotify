#include "SpotifyApiAccess.h"
#include "cpr/body.h"
#include "cpr/payload.h"
#include <string>
#include <tuple>

SpotifyApiAccess::SpotifyApiAccess()
{
    // todo: handle exception here aswell
    refreshAccessToken();
}

void SpotifyApiAccess::refreshAccessToken()
{
    const std::string query = "https://accounts.spotify.com/api/token";
    cpr::Response r = cpr::Post(
        cpr::Url(query),
        cpr::Payload{{"grant_type", "refresh_token"}, {"refresh_token", refresh_token}},
        cpr::Header{{"Authorization", "Basic " + base64}});

    // todo: check for response types (wrong access token, wrong request) first, instead of just throwing!
    if(r.error.code != cpr::ErrorCode::OK)
    {
        throw UnknownApiError();
    }

    auto r_json = json::parse(r.text);

    access_token = r_json["access_token"].get<std::string>();
}

std::tuple<std::vector<TrackData>, std::unordered_map<std::string, CoverInfo>>
SpotifyApiAccess::buildPlaylistData(const std::string& playlistID)
{
    cpr::Response r = cpr::Get(
        cpr::Url("https://api.spotify.com/v1/playlists/" + playlistID + "/tracks?limit=50&fields=total"),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
    auto total = json::parse(r.text)["total"].get<uint32_t>();

    std::vector<TrackData> playlistData;
    playlistData.reserve(total);
    std::unordered_map<std::string, CoverInfo> coverTable;

    // todo: could parallelize some of this (at least processing tracks from one request)

    // todo: not sure which of these is correct (if encoding here is even needed, but _encode() function
    // returns with \0 at end, so doesnt work), if error try utf8 encoded literal: u8", "
    // const std::string nameSeparator = utf8_encode(L", ");
    const std::string nameSeparator = ", ";
    int requestCountLimit = 50;
    GLuint freeLayer = 1;
    int iteration = 0;
    std::string queryUrl =
        "https://api.spotify.com/v1/playlists/" + playlistID +
        "/tracks?limit=" + std::to_string(requestCountLimit) +
        "&fields=next,items(track(name,id,artists(name),popularity,album(id,name,images)))";
    json r_json;
    r_json["next"] = queryUrl;
    do
    {
        // Load 50 *requestCountLimit tracks at once while there still tracks to be loaded
        const auto& a = r_json["next"];
        queryUrl = r_json["next"].get<std::string>();
        cpr::Response r = cpr::Get(
            cpr::Url(queryUrl),
            cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});

        // todo: check for response type (wrong access token, wrong request) first, instead of just throwing!
        if(r.error.code != cpr::ErrorCode::OK)
        {
            throw UnknownApiError();
        }
        r_json = json::parse(r.text);

        std::string ids;
        const int idLength = 22;               // Lengh of spotify IDs
        ids.reserve(total * idLength + total); // idLength characters per song + comma per song

        // load track data from first requst (names & ids)
        for(auto j = 0; j < r_json["items"].size(); j++)
        {
            const auto& track = r_json["items"][j]["track"];

            const auto& trackNameE = track["name"].get<std::string>();

            const auto& id = track["id"].get<std::string>();
            ids += id + ",";

            std::string artistsNamesE;
            for(auto i = 0; i < track["artists"].size(); i++)
            {
                const auto& artistNameE = track["artists"][i]["name"].get<std::string>();
                artistsNamesE += artistNameE;
                if(i < track["artists"].size() - 1)
                {
                    artistsNamesE += nameSeparator;
                }
            }

            const auto& albumId = track["album"]["id"].get<std::string>();
            const auto& albumNameE = track["album"]["name"].get<std::string>();

            auto& trackData = playlistData.emplace_back(
                iteration * requestCountLimit + j, id, trackNameE, artistsNamesE, albumId, albumNameE);
            trackData.features[8] = track["popularity"].get<int>() / 100.f;

            // create and/or link to album table
            auto it = coverTable.find(albumId);
            if(it == coverTable.end())
            {
                // not found, construct and set pointer
                // may need to pass defaultCoverHandle to function if setting outside doesnt work
                auto lastElement = [](const nlohmann::basic_json<>& json) -> const auto&
                {
                    return (*(--json.end()));
                };
                const std::string& coverUrl = lastElement(track["album"]["images"])["url"].get<std::string>();
                CoverInfo info{.url = coverUrl, .layer = freeLayer, .id = 420};
                freeLayer++;
                auto newEntry = coverTable.emplace(albumId, info);
                trackData.coverInfoPtr = &(newEntry.first->second);
            }
            else
            {
                trackData.coverInfoPtr = &(it->second);
            }
        }
        ids.pop_back();

        // Now retrieve audio featres from api (for the same batch of ids as the last request)
        queryUrl = "https://api.spotify.com/v1/audio-features?ids=" + ids;
        r = cpr::Get(
            cpr::Url(queryUrl),
            cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
        const auto feature_json = json::parse(r.text);
        for(auto i = 0; i < feature_json["audio_features"].size(); i++)
        {
            const int trackIndex = iteration * requestCountLimit + i;
            // ensure ids werent mixed up somehow
            assert(feature_json["audio_features"][i]["id"].get<std::string>() == playlistData[trackIndex].id);

            const auto& trackFeatures = feature_json["audio_features"][i];

            auto& trackData = playlistData[trackIndex];

            trackData.features[0] = trackFeatures["acousticness"];
            trackData.features[1] = trackFeatures["danceability"];
            trackData.features[2] = trackFeatures["energy"];
            trackData.features[3] = trackFeatures["instrumentalness"];
            trackData.features[4] = trackFeatures["speechiness"];
            trackData.features[5] = trackFeatures["liveness"];
            trackData.features[6] = trackFeatures["valence"];
            trackData.features[7] = trackFeatures["tempo"];
        }

        // std::cout << feature_json.dump(2) << std::endl;

        // std::cout << r_json.dump(2) << std::endl;
        // for(const auto& item : r_json["items"])
        // {
        //     std::cout << item["track"]["name"] << "\n";
        //     // std::wstring s = utf8_decode(item["track"]["artists"][0]["name"].get<std::string>());
        // }
        // std::cout << std::endl;
        iteration++;
    }
    while(!r_json["next"].is_null());

    return std::make_tuple(std::move(playlistData), std::move(coverTable));
}

json SpotifyApiAccess::getAlbum(const std::string& albumId)
{
    const std::string queryUrl = "https://api.spotify.com/v1/albums/" + albumId;
    cpr::Response r = cpr::Get(
        cpr::Url(queryUrl),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
    return json::parse(r.text);
}

void SpotifyApiAccess::stopPlayback()
{
    std::string queryUrl = "https://api.spotify.com/v1/me/player/pause";
    cpr::Response r = cpr::Put(
        cpr::Url(queryUrl),
        cpr::Header{
            {"Authorization", "Bearer " + access_token},
            {"Content-Type", "application/json"},
            {"Content-Length", "0"}});
    std::cout << r.text << std::endl;
}

// todo: handle no active device found (cache last active?)
void SpotifyApiAccess::startTrackPlayback(const std::string& trackId)
{
    // "load" the song into queue
    std::string queryUrl = "https://api.spotify.com/v1/me/player/queue?uri=spotify:track:" + trackId;
    cpr::Response r = cpr::Post(
        cpr::Url(queryUrl),
        cpr::Header{{"Authorization", "Bearer " + access_token}, {"Content-Type", "application/json"}});
    // std::cout << r.text << std::endl;

    // skip to that song
    queryUrl = "https://api.spotify.com/v1/me/player/next";
    r = cpr::Post(
        cpr::Url(queryUrl),
        cpr::Header{{"Authorization", "Bearer " + access_token}, {"Content-Type", "application/json"}});

    // now start the song
    // queryUrl = "https://api.spotify.com/v1/me/player/play";
    // r = cpr::Put(
    //     cpr::Url(queryUrl),
    //     cpr::Header{
    //         {"Authorization", "Bearer " + apiAccess.access_token},
    //         {"Content-Type", "application/json"},
    //         {"Content-Length", "0"}});
}

void SpotifyApiAccess::createPlaylist(const std::vector<std::string>& trackUris)
{
    // create new playlist
    const int MAXLEN = 80;
    char s[MAXLEN] = "PlaylistFilter generated playlist - ";
    time_t t = time(0);
    strftime(&s[36], MAXLEN, "%d/%m/%Y::%H:%M", localtime(&t));

    json body_json;
    body_json["name"] = s;
    body_json["public"] = false;
    std::string queryUrl = "https://api.spotify.com/v1/users/" + user_id + "/playlists";
    cpr::Response r = cpr::Post(
        cpr::Url(queryUrl),
        cpr::Header{{"Authorization", "Bearer " + access_token}, {"Content-Type", "application/json"}},
        cpr::Body{body_json.dump()});
    json ret_json = json::parse(r.text);
    std::string playlist_uri = ret_json["uri"].get<std::string>().substr(17, 22);

    // todo: progress bar
    queryUrl = "https://api.spotify.com/v1/playlists/" + playlist_uri + "/tracks";
    for(int i = 0; i < trackUris.size(); i += 100)
    {
        json uri_json;
        // uri_json["uris"] =
        // std::vector<std::string>{trackUris.begin(), trackUris.begin() + (trackUris.size())};
        for(int j = 0; j < 100 && (i + j) < trackUris.size(); j++)
        {
            uri_json["uris"].push_back(trackUris[i + j]);
        }
        std::cout << uri_json.dump(2) << std::endl;
        r = cpr::Post(
            cpr::Url(queryUrl),
            cpr::Header{{"Authorization", "Bearer " + access_token}, {"Content-Type", "application/json"}},
            cpr::Body{uri_json.dump()});
        std::cout << r.text << std::endl;
    }
}