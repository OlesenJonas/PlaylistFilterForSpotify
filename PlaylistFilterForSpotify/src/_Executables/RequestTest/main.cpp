#include <cpr/cpr.h>
#include <iostream>
#include <json.hpp>
#include <string>

#include "Spotify/SpotifyApiAccess.h"
#include "Track/Track.h"
#include "cpr/api.h"
#include "cpr/response.h"

using nlohmann::json;

int main()
{
    auto apiAccess = SpotifyApiAccess();
    const std::string playlist_test_id = "4yDYkPpEix7s5HK5ZBd7lz"; // art pop
    // const std::string playlist_test_id = "1Ck2gdMOFr5cbcuCbpV1sE"; //liked
    auto playlistData = apiAccess.buildPlaylistData(playlist_test_id);

    // url = "https://i.scdn.co/image/ab67616d00001e02e1530b42603367fdb2208d88"
    // response = requests.get(url)
    // img = Image.open(BytesIO(response.content))
    // img.show()

    std::string queryUrl = "https://i.scdn.co/image/ab67616d00001e02e1530b42603367fdb2208d88";
    cpr::Response r = cpr::Get(cpr::Url(queryUrl));
    // const auto feature_json = json::parse(r.text);
    // std::cout << r.text << std::endl;

    std::ofstream wf("C:/Users/jonas/Desktop/testimage.jpg", std::ios::out | std::ios::binary);
    if(!wf)
    {
        std::cout << "Cannot open file!" << std::endl;
        return 1;
    }
    // wf.write(r.text.c_str(), r.text.size());
    wf << r.text;
    wf.close();
    if(!wf.good())
    {
        std::cout << "Error occurred at writing time!" << std::endl;
        return 1;
    }
}