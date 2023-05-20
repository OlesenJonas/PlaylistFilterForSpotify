# PlaylistFilterForSpotify
Program to filter tracks of a Spotify playlist, aswell as visualize them in 3D.\
Useful for
- Selecting tracks with similar style from a playlist that contains a wider range of music.\
  Using Spotify's
  - Audio features
  - Recommendations
  - Genres

(These are my personal use-cases, and whats currently supported. For further features see: [WIP](#wip))\
Requires Opengl 4.5 support (could definitly be downgraded to a lower version with some code changes)

Filtering Overview
![Filtering Overview](images/filtering.png)

Visualization Overview
![Visualization Overview](images/plotting.png)

### Controls (of the 3D camera)
- Hold RMB to enter fly mode and use the mouse to look around + WASD and EQ(up/down) to move
- Hold MMB and move the mouse to rotate around the center. 
- Hold shift + MMB to pan the camera
- Scrollwheel to zoom in/out

---

### Building
Currently builds using Clang and CMake on Windows.\
Requires vcpkg for gathering the required packages. Its root directory must be stored in the *VCPKG_ROOT* environment variable.
Building from source requires you to provide your own Api Access through a ```secrets.hpp``` file in ```src/PlaylistFilter/Spotify/```. This file should define the following variables:
```cpp
static const std::string clientID = "...";

static const std::string base64 = "...";
// base64 contains base64_encode(clientID+":"+clientSecret)

static const std::string redirectURL = "...";
// eg: "https://olesenjonas.github.io/" 
// Needs to match the one defined in Spotify's app dashboard

static const std::string encodedRedirectURL = "...";
// eg: "https%3A%2F%2Folesenjonas.github.io%2F"
```
Client Secret and ID can be retrieved after registering an application at https://developer.spotify.com/dashboard/applications

#### Cross-Platform

Windows only code is used for two cases:
- string <-> wstring conversion using MultiByteToWideChar
- Opening URL using ShellExecute

Creates #error if \_WIN32 is not defined. Everything else should be cross-platform.

### WIP

These are things I think are useful but are not implemented yet (and may never be):\
(In no particular order)
- Error Handling (Spoiler: theres currently almost none, but really should be)\
  Especially for:
  - access-token timeout, which would be as simple as refreshing the token after 1h\
    thats how long they last currently, enough for me
  - api requests limit
- Fuzzy-String comparisons for searching
- Resize "Split" between both Tables
- Loading more Font ranges (and a Font that supports them) (Arabic, Chinese, etc.)
- Make loading the covers safer (currently pretty unsafe with .death()-ed threads, no guarantee they finish
- Hide individual columns of the tables
- Option to sort genre selection by # of occurance (current solution) *or* active/inactive
- Option to combine genres in different ways (currently track just needs to match *any* of the selected genres) but matching *all* genres could also be beneficial
- Better (looking) UI
