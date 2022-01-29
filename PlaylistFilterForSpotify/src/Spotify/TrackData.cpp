#include "TrackData.h"

TrackData::TrackData(
    int idx, std::string pid, std::string ptrackNameE, std::string partistsNamesE, std::string palbumId,
    std::string palbumNameE)
    : index(idx), id(pid), trackNameEncoded(ptrackNameE), artistsNamesEncoded(partistsNamesE),
      albumId(palbumId), albumNameEncoded(palbumNameE)
{
    // std::cout << "Constructing TrackData Object" << std::endl;
    trackName = utf8_decode(trackNameEncoded);
    artistsNames = utf8_decode(artistsNamesEncoded);
    albumName = utf8_decode(albumNameEncoded);
}