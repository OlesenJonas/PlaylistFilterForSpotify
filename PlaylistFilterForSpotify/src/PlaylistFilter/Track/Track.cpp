#include "Track.hpp"

#include <utility>

Track::Track(
    int idx,
    std::string pid,
    std::string ptrackNameE,
    std::string partistsNamesE,
    std::string palbumId,
    std::string palbumNameE)
    : index(idx),
      id(std::move(pid)),
      trackNameEncoded(std::move(ptrackNameE)),
      artistsNamesEncoded(std::move(partistsNamesE)),
      albumId(std::move(palbumId)),
      albumNameEncoded(std::move(palbumNameE))
{
    // std::cout << "Constructing TrackData Object" << std::endl;
    trackName = utf8_decode(trackNameEncoded);
    artistsNames = utf8_decode(artistsNamesEncoded);
    albumName = utf8_decode(albumNameEncoded);
}

void Track::decodeNames()
{
    trackName = utf8_decode(trackNameEncoded);
    artistsNames = utf8_decode(artistsNamesEncoded);
    albumName = utf8_decode(albumNameEncoded);
}

bool TrackSorter::operator()(const Track* td1, const Track* td2) const
{
    switch(index)
    {
    case 0:
        return td1->index < td2->index;
    case 4 ... 12:
        return td1->features[index - 4] < td2->features[index - 4];
    default:
        assert(0 && "Column Sorting not handled");
    }
    return td1->features[7] < td2->features[7]; // shouldnt be reached
};