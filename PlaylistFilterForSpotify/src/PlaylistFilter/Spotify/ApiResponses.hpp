#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct ResponseTotal
{
    uint32_t total;

    static ResponseTotal load(const std::string& text);
};

// ----

struct ImageResponse
{
    uint32_t height;
    uint32_t width;
    std::string url;
};

struct AlbumResponse
{
    std::string id;
    std::vector<ImageResponse> images;
    std::string name;
};

struct ArtistResponse
{
    std::string id;
    std::string name;
};

struct TrackResponse
{
    AlbumResponse album;
    std::vector<ArtistResponse> artists;
    std::string id;
    std::string name;
    uint32_t popularity;
};

struct PlaylistElementResponse
{
    TrackResponse track;
    // todo: could also be an episode, see API docs
};

struct PlaylistTracksResponse
{
    std::vector<PlaylistElementResponse> items;
    std::optional<std::string> next;

    static PlaylistTracksResponse load(const std::string& text);
};

// ----

struct AudioFeatureResponse
{
    std::string id;

    float acousticness;
    float danceability;
    float energy;
    float instrumentalness;
    float speechiness;
    float liveness;
    float valence;
    float tempo;
};

struct TracksFeaturesResponse
{
    std::vector<AudioFeatureResponse> audioFeatures;

    static TracksFeaturesResponse load(const std::string& text);
};

// ----

struct ArtistInfoResponse
{
    std::string id;
    std::vector<std::string> genres;
};

struct ArtistsResponse
{
    std::vector<ArtistInfoResponse> artists;

    static ArtistsResponse load(const std::string& text);
};