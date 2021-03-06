#include "SpotifyApiAccess.h"
#include "CommonStructs/CommonStructs.h"
#include "DynamicBitset/DynamicBitset.h"
#include "Spotify/secrets.h"
#include "cpr/body.h"
#include "cpr/payload.h"
#include "cpr/response.h"
#include <cguid.h>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>

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

std::tuple<std::vector<Track>, std::unordered_map<std::string, CoverInfo>, std::vector<std::string>>
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

    struct GenreData
    {
        uint32_t occurances = 0;
        uint32_t index = 0xFFFFFFFF;
        // dont store key directly, use value stored in key
        // cppreference says pointers stay alive even if iterators are invalidated
        const std::string* name;
    };
    struct ArtistData
    {
        std::vector<GenreData*> genres;
        std::string id;
        // dont store key directly, use value stored in key
        // cppreference says pointers stay alive even if iterators are invalidated
        const std::string* name;
        DynBitset* bitset;
    };
    std::unordered_map<std::string, ArtistData> artists;
    // dont want to save this as part of track (atm its not needed later on, so save externally here)
    std::vector<std::vector<ArtistData*>> artistsPerTrack(total);

    // todo: could parallelize some of this (at least processing tracks from one request)

    const std::string nameSeparator = ", ";
    int requestCountLimit = 50;
    int iteration = 0;
    std::string queryUrl =
        "https://api.spotify.com/v1/playlists/" + std::string(playlistID) +
        "/tracks?limit=" + std::to_string(requestCountLimit) +
        "&fields=next,items(track(name,id,artists(name,id),popularity,album(id,name,images)))";
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
            const int trackIndex = iteration * requestCountLimit + j;
            const auto& track_json = r_json["items"][j]["track"];

            const auto& trackNameE = track_json["name"].get<std::string>();

            const auto& id = track_json["id"].get<std::string>();
            ids += id + ",";

            std::string artistsNamesE;
            artistsPerTrack[trackIndex].resize(track_json["artists"].size(), nullptr);
            for(auto i = 0; i < track_json["artists"].size(); i++)
            {
                const auto& artistNameE = track_json["artists"][i]["name"].get<std::string>();
                artistsNamesE += artistNameE;
                if(i < track_json["artists"].size() - 1)
                {
                    artistsNamesE += nameSeparator;
                }

                // Save artist to retrieve genres later
                auto artistEmplaceResult = artists.emplace(
                    std::pair<std::string, ArtistData>(artistNameE, {{}, "", nullptr, nullptr}));
                ArtistData& artistData = artistEmplaceResult.first->second;
                artistsPerTrack[trackIndex][i] = &artistData;
                if(artistEmplaceResult.second)
                {
                    // artist was newly inserted
                    artistData.name = &artistEmplaceResult.first->first;
                    artistData.id = track_json["artists"][i]["id"].get<std::string>();
                }
                else
                {
                    // artist was already in map
                    assert(artistData.name != nullptr);
                    assert(!artistData.id.empty());
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

    std::unordered_map<std::string, GenreData> genres;
    // now get all genres by retrieving all atrist information
    std::string ids;
    // can request up to 50 ids at once (id length is 22) + comma after each
    ids.reserve(50 * 22 + 50);
    // need a copy of the iterator before the request for a 2nd iteration through the elements
    auto requestBeginIt = artists.begin();
    while(true)
    {
        auto iter = requestBeginIt;
        int requestSize = 0;
        for(; requestSize < 50 && iter != artists.end(); requestSize++)
        {
            ids += iter->second.id;
            ids += ',';
            iter++;
        }
        ids.pop_back(); // delete trailing comma
        queryUrl = "https://api.spotify.com/v1/artists?ids=" + ids;
        cpr::Response r = cpr::Get(
            cpr::Url(queryUrl),
            cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
        const auto artists_json = json::parse(r.text);
        auto answerSize = artists_json["artists"].size();
        assert(requestSize == answerSize);
        // iterator backup needed here to step through request response
        iter = requestBeginIt;
        for(int i = 0; i < requestSize; i++)
        {
            ArtistData& artist = iter->second;
            const auto& artistJson = artists_json["artists"][i];
            const auto& artistName = artistJson["name"].get<std::string>();
            const auto& artistId = artistJson["id"].get<std::string>();
            assert(artistName == *artist.name);
            assert(artistId == artist.id);
            for(int k = 0; k < artistJson["genres"].size(); k++)
            {
                const auto& genreName = artistJson["genres"][k].get<std::string>();
                auto genreEmplaceResult =
                    genres.emplace(std::pair<std::string, GenreData>(genreName, {0, 0xFFFFFFFF, nullptr}));
                GenreData& genreData = genreEmplaceResult.first->second;
                if(genreEmplaceResult.second)
                {
                    genreData.name = &genreEmplaceResult.first->first;
                }
                assert(genreData.name != nullptr);
                artist.genres.push_back(&genreEmplaceResult.first->second);
                genreData.occurances++;
            }
            iter++;
        }

        requestBeginIt = iter;
        ids.clear();

        if(iter == artists.end())
        {
            break;
        }
    }
    std::vector<GenreData*> genreProxy;
    genreProxy.reserve(genres.size());
    for(auto& genreData : genres)
    {
        genreProxy.emplace_back(&genreData.second);
    }
    std::sort(
        genreProxy.begin(),
        genreProxy.end(),
        [](const GenreData* lhs, const GenreData* rhs) -> bool
        {
            return lhs->occurances > rhs->occurances;
        });
    for(uint32_t i = 0; i < genreProxy.size(); i++)
    {
        genreProxy[i]->index = i;
    }
    std::vector<DynBitset> artistsBitsets(artists.size(), DynBitset{static_cast<uint32_t>(genres.size())});
    int i = 0;
    for(auto& artistDataIter : artists)
    {
        auto& artistData = artistDataIter.second;
        artistData.bitset = &artistsBitsets[i];
        for(const GenreData* genre : artistData.genres)
        {
            artistData.bitset->setBit(genre->index);
        }
        i++;
    }

    for(auto i = 0; i < playlist.size(); i++)
    {
        Track& track = playlist[i];
        track.genreMask = DynBitset(genres.size());
        for(const ArtistData* artistPtr : artistsPerTrack[i])
        {
            // todo: maybe "and" combination of artist genres better
            //       but can think of cases for either option where its wrong/suboptimal
            track.genreMask = track.genreMask | *artistPtr->bitset;
        }
    }

    std::vector<std::string> sortedCategories(genres.size());
    for(int i = 0; i < genreProxy.size(); i++)
    {
        auto genreEntry = genres.extract(*genreProxy[i]->name);
        sortedCategories[i] = std::move(genreEntry.key());
        // move string from key of map, map isnt needed anymore anyways
        // sortedCategories[i] = std::move(*const_cast<std::string*>(genreProxy[i]->name));
    }

    return std::make_tuple(std::move(playlist), std::move(coverTable), std::move(sortedCategories));
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
bool SpotifyApiAccess::startTrackPlayback(const std::string& trackId)
{
    // "load" the song into queue
    std::string queryUrl = "https://api.spotify.com/v1/me/player/queue?uri=spotify:track:" + trackId;
    cpr::Response r = cpr::Post(
        cpr::Url(queryUrl),
        cpr::Header{{"Authorization", "Bearer " + access_token}, {"Content-Type", "application/json"}});
    if(r.status_code == 404)
    {
        return false;
    }

    // skip to that song, starts plaback automatically it seems
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
    return true;
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