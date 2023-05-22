#pragma once

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include <CommonStructs/CommonStructs.hpp>
#include <DynamicBitset/DynamicBitset.hpp>
#include <utils/utf.hpp>

struct Track
{
    Track() = default;
    Track(
        int idx,
        std::string pid,
        std::string ptrackNameE,
        std::string partistsNamesE,
        std::string palbumId,
        std::string palbumNameE);
    int index = -1;

    std::string id;

    std::string trackNameEncoded;
    std::wstring trackName;

    std::string artistsNamesEncoded;
    std::wstring artistsNames;

    std::string albumId;
    std::string albumNameEncoded;
    std::wstring albumName;

    // details see:
    // https://developer.spotify.com/documentation/web-api/reference/#/operations/get-several-audio-features
    static constexpr int featureAmount = 9;
    // keep full string here because its needed for the imgui widgets, makes next part super ugly though
    static constexpr char* FeatureNamesData = "Acousticness\0Danceability\0Energy\0Instrumentalness\0Speechin"
                                              "ess\0Liveness\0Valence\0Tempo*\0Popularity\0";
    // ugly way of defining the sections, probably(?) doable with macro/template
    static constexpr std::array<std::string_view, featureAmount> FeatureNames = {
        {{&FeatureNamesData[0]},
         {&FeatureNamesData[13]},
         {&FeatureNamesData[26]},
         {&FeatureNamesData[33]},
         {&FeatureNamesData[50]},
         {&FeatureNamesData[62]},
         {&FeatureNamesData[71]},
         {&FeatureNamesData[79]},
         {&FeatureNamesData[86]}}};
    std::array<float, featureAmount> features;

    CoverInfo* coverInfoPtr = nullptr;

    DynBitset genreMask;
    DynBitset artistMask;

    void decodeNames();
};

struct TrackSorter
{
    explicit TrackSorter(int i) : index(i){};
    bool operator()(const Track* td1, const Track* td2) const;

  private:
    int index;
};