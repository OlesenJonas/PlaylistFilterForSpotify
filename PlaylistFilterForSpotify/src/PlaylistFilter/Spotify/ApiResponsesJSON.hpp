#pragma once

#include "ApiResponses.hpp"
#include "Spotify/ApiResponses.hpp"

#include <daw/json/daw_json_link.h>

#define JSONType(T)                                                                                               \
    template <>                                                                                                   \
    struct daw::json::json_data_contract<T>

JSONType(ResponseTotal)
{
    using type = json_member_list<     //
        json_number<"total", uint32_t> //
        >;
};

// ----

JSONType(ImageResponse)
{
    using type = json_member_list<       //
        json_number<"height", uint32_t>, //
        json_number<"width", uint32_t>,  //
        json_string<"url">               //
        >;
};

JSONType(AlbumResponse)
{
    using type = json_member_list<           //
        json_string<"id">,                   //
        json_array<"images", ImageResponse>, //
        json_string<"name">                  //
        >;
};

JSONType(ArtistResponse)
{
    using type = json_member_list< //
        json_string<"id">,         //
        json_string<"name">        //
        >;
};

JSONType(TrackResponse)
{
    using type = json_member_list<             //
        json_class<"album", AlbumResponse>,    //
        json_array<"artists", ArtistResponse>, //
        json_string<"id">,                     //
        json_string<"name">,                   //
        json_number<"popularity", uint32_t>    //
        >;
};

JSONType(PlaylistElementResponse)
{
    using type = json_member_list<         //
        json_class<"track", TrackResponse> //
        >;
};

JSONType(PlaylistTracksResponse)
{
    using type = json_member_list<                    //
        json_array<"items", PlaylistElementResponse>, //
        json_string_null<"next">                      //
        >;
};

// ----

JSONType(AudioFeatureResponse)
{
    using type = json_member_list<              //
        json_string<"id">,                      //
        json_number<"acousticness", float>,     //
        json_number<"danceability", float>,     //
        json_number<"energy", float>,           //
        json_number<"instrumentalness", float>, //
        json_number<"speechiness", float>,      //
        json_number<"liveness", float>,         //
        json_number<"valence", float>,          //
        json_number<"tempo", float>             //
        >;
};

JSONType(TracksFeaturesResponse)
{
    using type = json_member_list<                         //
        json_array<"audio_features", AudioFeatureResponse> //
        >;
};

// ----

JSONType(ArtistInfoResponse)
{
    using type = json_member_list<        //
        json_string<"id">,                //
        json_array<"genres", std::string> //
        >;
};

JSONType(ArtistsResponse)
{
    using type = json_member_list<                //
        json_array<"artists", ArtistInfoResponse> //
        >;
};