#include "ApiResponsesJSON.hpp"

ResponseTotal ResponseTotal::load(const std::string& text)
{
    return daw::json::from_json<ResponseTotal>(text);
}
PlaylistTracksResponse PlaylistTracksResponse::load(const std::string& text)
{
    return daw::json::from_json<PlaylistTracksResponse>(text);
}
TracksFeaturesResponse TracksFeaturesResponse::load(const std::string& text)
{
    return daw::json::from_json<TracksFeaturesResponse>(text);
}
ArtistsResponse ArtistsResponse::load(const std::string& text)
{
    return daw::json::from_json<ArtistsResponse>(text);
}