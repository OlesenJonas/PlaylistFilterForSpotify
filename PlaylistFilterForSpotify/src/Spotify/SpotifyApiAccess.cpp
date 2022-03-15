#include "SpotifyApiAccess.h"
#include "ShaderProgram/ShaderProgram.h"
#include "Spotify/secrets.h"
#include "cpr/body.h"
#include "cpr/payload.h"
#include "cpr/response.h"
#include <cguid.h>
#include <string>
#include <tuple>

#include <cryptopp/base64.h>
#include <cryptopp/integer.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>

SpotifyApiAccess::SpotifyApiAccess()
{
}

std::string SpotifyApiAccess::getAuthURL()
{
    CryptoPP::AutoSeededRandomPool rng;
    static constexpr std::string_view pool = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890";

    state.resize(10);
    for(char& c : state)
    {
        const CryptoPP::Integer indxI(rng, 0, pool.size() - 1);
        c = pool[indxI.ConvertToLong()];
    }

    const CryptoPP::Integer verifierLength(rng, 43, 128);
    code_verifier.resize(verifierLength.ConvertToLong());

    for(char& c : code_verifier)
    {
        const CryptoPP::Integer indxI(rng, 0, pool.size() - 1);
        c = pool[indxI.ConvertToLong()];
    }

    CryptoPP::SHA256 hash;
    hash.Update(reinterpret_cast<const CryptoPP::byte*>(code_verifier.data()), code_verifier.size());
    std::vector<CryptoPP::byte> digest(hash.DigestSize());
    hash.Final(digest.data());
    std::string code_challenge;
    CryptoPP::Base64URLEncoder encoder;
    encoder.Put(digest.data(), digest.size());
    encoder.MessageEnd();
    code_challenge.resize(encoder.MaxRetrievable());
    encoder.Get(reinterpret_cast<CryptoPP::byte*>(&code_challenge[0]), code_challenge.size());

    return "https://accounts.spotify.com/authorize?" +                                         //
           ("client_id=" + clientID +                                                          //
            "&response_type=code" +                                                            //
            "&redirect_uri=" + encodedRedirectURL +                                            //
            "&state=" + state +                                                                //
            "&scope=user-modify-playback-state%20user-library-read%20playlist-modify-private%" //
            "20playlist-modify-public" +                                                       //
            "&show_dialog=true" +                                                              //
            "&code_challenge_method=S256" +                                                    //
            "&code_challenge=" +
            code_challenge);
}

bool SpotifyApiAccess::checkAuth(const std::string& p_state, const std::string& code)
{
    if(p_state != state)
    {
        return false;
    }

    std::string query = "https://accounts.spotify.com/api/token";
    cpr::Response r = cpr::Post(
        cpr::Url(query),
        cpr::Payload{
            {"grant_type", "authorization_code"},
            {"code", code},
            {"redirect_uri", redirectURL},
            {"client_id", clientID},
            {"code_verifier", code_verifier}},
        cpr::Header{{"Authorization", "Basic " + base64}});
    if(r.status_code != 200)
    {
        std::cout << r.text << std::endl;
        return false;
    }
    json r_json = json::parse(r.text);
    access_token = r_json["access_token"].get<std::string>();
    refresh_token = r_json["refresh_token"].get<std::string>();

    // get user id
    r = cpr::Get(
        cpr::Url("https://api.spotify.com/v1/me"),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
    r_json = json::parse(r.text);
    userId = r_json["id"].get<std::string>();

    return true;
}

void SpotifyApiAccess::refreshAccessToken()
{
    const std::string query = "https://accounts.spotify.com/api/token";
    cpr::Response r = cpr::Post(
        cpr::Url(query),
        cpr::Payload{
            {"grant_type", "refresh_token"}, {"refresh_token", refresh_token}, {"client_id", clientID}},
        cpr::Header{{"Authorization", "Basic " + base64}});

    auto r_json = json::parse(r.text);

    access_token = r_json["access_token"].get<std::string>();
}

std::tuple<std::vector<Track>, std::unordered_map<std::string, CoverInfo>>
SpotifyApiAccess::buildPlaylistData(std::string_view playlistID, float* progressTracker)
{
    cpr::Response r = cpr::Get(
        cpr::Url(
            "https://api.spotify.com/v1/playlists/" + std::string(playlistID) +
            "/tracks?limit=50&fields=total"),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
    auto total = json::parse(r.text)["total"].get<uint32_t>();
    uint32_t tracksLoaded = 0;

    std::vector<Track> playlist;
    playlist.reserve(total);
    std::unordered_map<std::string, CoverInfo> coverTable;

    // todo: could parallelize some of this (at least processing tracks from one request)

    const std::string nameSeparator = ", ";
    int requestCountLimit = 50;
    int iteration = 0;
    std::string queryUrl =
        "https://api.spotify.com/v1/playlists/" + std::string(playlistID) +
        "/tracks?limit=" + std::to_string(requestCountLimit) +
        "&fields=next,items(track(name,id,artists(name),popularity,album(id,name,images)))";
    json r_json;
    r_json["next"] = queryUrl;
    do
    {
        // Load requestCountLimit (50) tracks at once while there still tracks to be loaded
        const auto& a = r_json["next"];
        queryUrl = r_json["next"].get<std::string>();
        cpr::Response r = cpr::Get(
            cpr::Url(queryUrl),
            cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});

        // todo: check for response type (wrong access token, wrong request) first, instead of just throwing!
        if(r.status_code == 429)
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
            const auto& track_json = r_json["items"][j]["track"];

            const auto& trackNameE = track_json["name"].get<std::string>();

            const auto& id = track_json["id"].get<std::string>();
            ids += id + ",";

            std::string artistsNamesE;
            for(auto i = 0; i < track_json["artists"].size(); i++)
            {
                const auto& artistNameE = track_json["artists"][i]["name"].get<std::string>();
                artistsNamesE += artistNameE;
                if(i < track_json["artists"].size() - 1)
                {
                    artistsNamesE += nameSeparator;
                }
            }

            const auto& albumId = track_json["album"]["id"].get<std::string>();
            const auto& albumNameE = track_json["album"]["name"].get<std::string>();

            auto& track = playlist.emplace_back(
                iteration * requestCountLimit + j, id, trackNameE, artistsNamesE, albumId, albumNameE);
            track.features[8] = track_json["popularity"].get<int>() / 100.f;

            auto lastElement = [](const nlohmann::basic_json<>& json) -> const auto&
            {
                return (*(--json.end()));
            };
            // create and/or link to album table
            auto it = coverTable.find(albumId);
            if(it == coverTable.end())
            {
                // not found, construct and set pointer
                const std::string& coverUrl =
                    lastElement(track_json["album"]["images"])["url"].get<std::string>();
                CoverInfo info{.url = coverUrl, .layer = 0, .id = 0xFFFFFFFFu};
                auto newEntry = coverTable.emplace(albumId, info);
                track.coverInfoPtr = &(newEntry.first->second);
            }
            else
            {
                track.coverInfoPtr = &(it->second);
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
            assert(feature_json["audio_features"][i]["id"].get<std::string>() == playlist[trackIndex].id);

            const auto& trackFeatures = feature_json["audio_features"][i];

            auto& track = playlist[trackIndex];

            track.features[0] = trackFeatures["acousticness"];
            track.features[1] = trackFeatures["danceability"];
            track.features[2] = trackFeatures["energy"];
            track.features[3] = trackFeatures["instrumentalness"];
            track.features[4] = trackFeatures["speechiness"];
            track.features[5] = trackFeatures["liveness"];
            track.features[6] = trackFeatures["valence"];
            track.features[7] = trackFeatures["tempo"];
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

        tracksLoaded += requestCountLimit;
        *progressTracker = static_cast<float>(tracksLoaded) / static_cast<float>(total);
        *progressTracker = 0.9f * std::min(*progressTracker, 1.0f);
    }
    while(!r_json["next"].is_null());

    return std::make_tuple(std::move(playlist), std::move(coverTable));
}

json SpotifyApiAccess::getAlbum(const std::string& albumId)
{
    const std::string queryUrl = "https://api.spotify.com/v1/albums/" + albumId;
    cpr::Response r = cpr::Get(
        cpr::Url(queryUrl),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
    return json::parse(r.text);
}

std::optional<std::string> SpotifyApiAccess::checkPlaylistExistance(std::string_view id)
{
    cpr::Response r = cpr::Get(
        cpr::Url("https://api.spotify.com/v1/playlists/" + std::string(id) + "?fields=name"),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
    if(r.status_code == 200)
    {
        return json::parse(r.text)["name"].get<std::string>();
    }
    // handle case of Api timeout, not every non 200 code means playlist doesnt exist
    return {};
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

void SpotifyApiAccess::createPlaylist(std::string_view name, const std::vector<std::string>& trackUris)
{
    // create new playlist
    json body_json;
    body_json["name"] = name;
    body_json["public"] = false;
    std::string queryUrl = "https://api.spotify.com/v1/users/" + userId + "/playlists";
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

std::vector<std::string> SpotifyApiAccess::getRecommendations(std::vector<std::string_view>& seedIds)
{
    assert(seedIds.size() <= 5);
    std::string seedString;
    // length of Id(22) characters per song + comma per song (excpt last)
    seedString.reserve(seedIds.size() * 22 + (seedIds.size() - 1));
    for(int i = 0; i < seedIds.size(); i++)
    {
        seedString += seedIds[i];
        if(i != seedIds.size() - 1)
        {
            seedString += ",";
        }
    }
    cpr::Response r = cpr::Get(
        cpr::Url("https://api.spotify.com/v1/recommendations?limit=100&seed_tracks=" + seedString),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
    json r_json = json::parse(r.text);

    std::vector<std::string> result = {};
    result.reserve(r_json["tracks"].size());
    for(const auto& track : r_json["tracks"])
    {
        // std::cout << track["name"].get<std::string>() << "\n";
        result.emplace_back(track["id"].get<std::string>());
    }
    // std::cout << std::endl << std::endl;
    return result;
}