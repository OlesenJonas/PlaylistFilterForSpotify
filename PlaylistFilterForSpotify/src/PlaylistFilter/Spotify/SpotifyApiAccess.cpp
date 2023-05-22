#include "SpotifyApiAccess.hpp"
#include "ApiError.hpp"
#include "ApiResponses.hpp"
#include "Spotify/ApiResponses.hpp"
#include "secrets.hpp"
#include <CommonStructs/CommonStructs.hpp>
#include <DynamicBitset/DynamicBitset.hpp>

#include <cpr/body.h>
#include <cpr/payload.h>
#include <cpr/response.h>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
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
        cpr::Payload{{"grant_type", "refresh_token"}, {"refresh_token", refresh_token}, {"client_id", clientID}},
        cpr::Header{{"Authorization", "Basic " + base64}});

    auto r_json = json::parse(r.text);

    access_token = r_json["access_token"].get<std::string>();
}

std::tuple<std::vector<Track>, SpotifyApiAccess::CoverTable_t, std::vector<std::string>>
SpotifyApiAccess::buildPlaylistData(std::string_view playlistID, float* progressTracker, std::string* progressName)
{
    cpr::Response r = cpr::Get(
        cpr::Url(
            "https://api.spotify.com/v1/playlists/" + std::string(playlistID) + "/tracks?limit=50&fields=total"),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});

    ResponseTotal responseTotal = ResponseTotal::load(r.text);
    uint32_t totalAmountOfTracks = responseTotal.total;
    int requestCountLimit = 50;
    uint32_t trackRequestsAmount = (totalAmountOfTracks + requestCountLimit - 1) / requestCountLimit;

    uint32_t tracksLoaded = 0;

    std::vector<Track> tracks;
    tracks.reserve(totalAmountOfTracks);

    CoverTable_t coverTable;

    // todo: Doesnt work if an item in the playlist is an episode instead of a song!
    std::string initialQuery =
        "https://api.spotify.com/v1/playlists/" + std::string(playlistID) +
        "/tracks?limit=" + std::to_string(requestCountLimit) +
        "&fields=next,items(track(name,id,artists(name,id),popularity,album(id,name,images)))";
    PlaylistTracksResponse response;

    uint32_t freeArtistIDIndex = 0;
    std::vector<uint32_t> artistOccurances;
    std::unordered_map<ArtistID, uint32_t, StringHash, std::equal_to<>> artistIDtoIndex;
    // the actual vector will be created later, so these indices arent valid until then!
    std::vector<std::vector<uint32_t>> perTrackArtistIndices;
    perTrackArtistIndices.resize(totalAmountOfTracks);

    TracksFeaturesResponse audioFeatureResponse;
    for(int i = 0; i < trackRequestsAmount; i++)
    {
        if(i != 0)
            assert(response.next.has_value());
        std::string queryURL = i == 0 ? initialQuery : response.next.value();

        cpr::Response r = cpr::Get(
            cpr::Url(queryURL),
            cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});

        // todo: check for response type (wrong access token, wrong request) first, instead of just throwing!
        if(r.status_code == 429)
        {
            throw UnknownApiError();
        }
        response = PlaylistTracksResponse::load(r.text);
        // now that all responses are being stored, shrink to fit?

        uint32_t tracksInRequest = response.items.size();

        // todo: could also move this outside of loop! (just cant use pop_back anymore)
        std::string trackIds;
        // Lengh of spotify IDs (I think, but cant find specification for it in API atm)
        const int idLength = 22;
        // idLength characters per song + comma per song
        trackIds.reserve(tracksInRequest * idLength + tracksInRequest);

        // TODO: I think a lot of the strings can be moved instead of copied

        // load track data from first requst (names & ids)
        for(auto j = 0; j < tracksInRequest; j++)
        {
            const int trackIndex = i * requestCountLimit + j;
            const auto& trackResponse = response.items[j].track;
            Track& track = tracks.emplace_back();
            assert(std::distance(tracks.data(), &track) == trackIndex);
            track.index = trackIndex;

            // can move here?
            track.trackNameEncoded = trackResponse.name;
            track.id = trackResponse.id;

            assert(track.id.length() == 22);
            trackIds += track.id + ",";

            std::string& artistsNamesE = track.artistsNamesEncoded;

            perTrackArtistIndices[trackIndex].resize(trackResponse.artists.size());
            for(auto i = 0; i < trackResponse.artists.size(); i++)
            {
                auto& artist = trackResponse.artists[i];
                const auto& artistNameE = artist.name;
                artistsNamesE += artistNameE;
                if(i < trackResponse.artists.size() - 1)
                {
                    artistsNamesE += ", ";
                }

                assert(artist.id.size() == 22);
                auto artistIdToIndexIter = artistIDtoIndex.find(artist.id);
                if(artistIdToIndexIter == artistIDtoIndex.end())
                {
                    // artist hasnt been recorded yet, add entry
                    auto newEntry = artistIDtoIndex.emplace(std::make_pair(artist.id, freeArtistIDIndex));
                    assert(newEntry.second);
                    assert(newEntry.first->second == artistOccurances.size());
                    freeArtistIDIndex++;
                    artistOccurances.emplace_back(0);

                    artistIdToIndexIter = newEntry.first;
                }
                artistOccurances[artistIdToIndexIter->second]++;
                perTrackArtistIndices[trackIndex][i] = artistIdToIndexIter->second;
            }

            track.albumId = trackResponse.album.id;
            track.albumNameEncoded = trackResponse.album.name;

            track.features[8] = trackResponse.popularity / 100.f;

            // create and/or link to album table
            auto iter = coverTable.find(track.albumId);
            if(iter == coverTable.end())
            {
                // not found, construct and set pointer
                assert(track.albumId.size() == 22);
                const std::string& coverUrl =
                    trackResponse.album.images[trackResponse.album.images.size() - 1].url;
                // todo: pretty sure can also move here
                CoverInfo info{.url = coverUrl, .layer = 0, .id = 0xFFFFFFFFu};
                auto newEntry = coverTable.emplace(track.albumId, info);
                track.coverInfoPtr = &(newEntry.first->second);
            }
            else
            {
                track.coverInfoPtr = &(iter->second);
            }

            track.decodeNames();
        }
        // remove trailing comma from track id list
        trackIds.pop_back();

        // Now retrieve audio featres from api (for the same batch of ids as the playlist tracks request)
        queryURL = "https://api.spotify.com/v1/audio-features?ids=" + trackIds;
        r = cpr::Get(
            cpr::Url(queryURL),
            cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
        audioFeatureResponse = TracksFeaturesResponse::load(r.text);
        for(auto j = 0; j < audioFeatureResponse.audioFeatures.size(); j++)
        {
            const int trackIndex = i * requestCountLimit + j;
            // ensure ids werent mixed up somehow
            assert(audioFeatureResponse.audioFeatures[j].id == tracks[trackIndex].id);

            const auto& trackFeatures = audioFeatureResponse.audioFeatures[j];

            Track& track = tracks[trackIndex];

            track.features[0] = trackFeatures.acousticness;
            track.features[1] = trackFeatures.danceability;
            track.features[2] = trackFeatures.energy;
            track.features[3] = trackFeatures.instrumentalness;
            track.features[4] = trackFeatures.speechiness;
            track.features[5] = trackFeatures.liveness;
            track.features[6] = trackFeatures.valence;
            track.features[7] = trackFeatures.tempo;
        }

        tracksLoaded += requestCountLimit;
        *progressTracker = static_cast<float>(tracksLoaded) / static_cast<float>(totalAmountOfTracks);
        *progressTracker = std::min(*progressTracker, 1.0f);
    }

    *progressName = "Analyzing genres";

    // The genres need to be fetched from a different endpoint
    // They also need to be mapped from artists -> tracks and sorted by occurance

    struct GenreData
    {
        uint32_t occurances = 0;
        uint32_t sortedIndex = 0;
        const std::string* name = nullptr;
    };
    // store all genres in a nice linear array, need to have stability which index offers
    std::vector<GenreData> genreData;
    std::unordered_map<GenreName, uint32_t> genreNameToIndex;

    std::vector<std::vector<uint32_t>> perArtistGenreIndices;
    perArtistGenreIndices.resize(artistIDtoIndex.size());

    // again, request up to 50 ids at once
    std::string ids;
    ids.reserve(50 * 22 + 50);

    *progressName = "Downloading genre data";
    ArtistsResponse artistsResponse;
    auto iter = artistIDtoIndex.begin();
    assert(artistOccurances.size() == artistIDtoIndex.size());
    uint32_t artistCount = artistOccurances.size();

    std::vector<uint32_t> artistIndices;
    artistIndices.resize(50);
    for(int i = 0; i < artistCount; i += 50)
    {
        int requestSize = 0;
        for(; requestSize < 50 && iter != artistIDtoIndex.end(); requestSize++)
        {
            ids += iter->first;
            ids += ',';
            artistIndices[requestSize] = iter->second;
            iter++;
        }
        ids.pop_back(); // delete trailing comma

        std::string queryURL = "https://api.spotify.com/v1/artists?ids=" + ids;
        cpr::Response r = cpr::Get(
            cpr::Url(queryURL),
            cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
        artistsResponse = ArtistsResponse::load(r.text);
        assert(requestSize == artistsResponse.artists.size());

        auto& artists = artistsResponse.artists;
        for(int j = 0; j < artists.size(); j++)
        {
            auto& artist = artists[j];
            uint32_t& artistIndex = artistIndices[j];

            assert(artistIDtoIndex.find(artist.id)->second == artistIndex);
            perArtistGenreIndices[artistIndex].reserve(artist.genres.size());

            for(auto& genreName : artist.genres)
            {
                auto genreNameToIndexIter = genreNameToIndex.find(genreName);
                if(genreNameToIndexIter == genreNameToIndex.end())
                {
                    // genre hasnt been recorded yet

                    // add genre to vector
                    GenreData& newGenreData = genreData.emplace_back();
                    uint32_t newIndex = genreData.size() - 1;
                    assert(&genreData[newIndex] == &newGenreData);

                    // add new mapping: name -> new index
                    auto emplaceResult = genreNameToIndex.emplace(std::make_pair(genreName, newIndex));
                    assert(emplaceResult.second);
                    genreNameToIndexIter = emplaceResult.first;
                }
                genreData[genreNameToIndexIter->second].occurances += artistOccurances[artistIndex];
                // add reference back to artist!
                perArtistGenreIndices[artistIndex].emplace_back(genreNameToIndexIter->second);
            }
        }

        *progressTracker = static_cast<float>(i) / static_cast<float>(artistCount);

        ids.clear();
    }
    *progressTracker = 1.0f;

    *progressName = "Sorting genres";
    for(auto& entry : genreNameToIndex)
    {
        genreData[entry.second].name = &entry.first;
    }

    std::vector<GenreData*> genreProxies;
    genreProxies.reserve(genreData.size());
    for(auto& data : genreData)
    {
        genreProxies.emplace_back(&data);
    }
    std::sort(
        genreProxies.begin(),
        genreProxies.end(),
        [](const GenreData* lhs, const GenreData* rhs) -> bool { return lhs->occurances > rhs->occurances; });

    for(uint32_t i = 0; i < genreProxies.size(); i++)
    {
        genreProxies[i]->sortedIndex = i;
    }

    // Could see if its worth it to generate a bitset for each artist first, and then for each track just combine
    // all artists bitsets
    // But for now, just do a simple double loop. (at least this definitly uses less memory)
    assert(genreData.size() == genreNameToIndex.size());
    for(int i = 0; i < tracks.size(); i++)
    {
        Track& track = tracks[i];
        track.genreMask = DynBitset{static_cast<uint32_t>(genreData.size())};
        for(uint32_t& artistIndex : perTrackArtistIndices[i])
        {
            for(uint32_t& genreDataIndex : perArtistGenreIndices[artistIndex])
            {
                const GenreData& data = genreData[genreDataIndex];
                track.genreMask.setBit(data.sortedIndex);
            }
        }
    }

    // this is last so can move into
    std::vector<std::string> sortedGenres;
    sortedGenres.resize(genreData.size());
    for(auto& genreProxy : genreProxies)
    {
        // this will break the map, but its useless now anyways
        sortedGenres[genreProxy->sortedIndex] = *genreProxy->name;
    }

    return std::make_tuple(std::move(tracks), std::move(coverTable), std::move(sortedGenres));
}

json SpotifyApiAccess::getAlbum(const std::string& albumId)
{
    const std::string queryUrl = "https://api.spotify.com/v1/albums/" + albumId;
    cpr::Response r = cpr::Get(
        cpr::Url(queryUrl),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
    return json::parse(r.text);
}

std::string SpotifyApiAccess::checkPlaylistExistance(std::string_view id)
{
    cpr::Response r = cpr::Get(
        cpr::Url("https://api.spotify.com/v1/playlists/" + std::string(id) + "?fields=name"),
        cpr::Header{{"Content-Type", "application/json"}, {"Authorization", "Bearer " + access_token}});
    if(r.status_code == 200)
    {
        return json::parse(r.text)["name"].get<std::string>();
    }
    // handle case of Api timeout, not every non 200 code means playlist doesnt exist
    return "";
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