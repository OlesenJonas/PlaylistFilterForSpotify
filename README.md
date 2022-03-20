# PlaylistFilterForSpotify
Program to filter tracks of a Spotify playlist aswell as visualize them in 3D.\
Useful for
- selecting a group of tracks with similar style from a playlist that contains multiple types of music
  - based on Audio features
  - and/or Spotify recommendations
- visualizing the audio properties of a playlist in 3D

(These are my personal use-cases, and whats currently supported. For further features see: [WIP](#wip))\
Requires Opengl 4.5 support (could definitly be downgraded to a lower version with some code changes)

Filtering Overview
![Filtering Overview](images/filtering.png)

Visualization Overview
![Visualization Overview](images/plotting.png)

### Controls (for the camera in 3D)
- Hold RMB to enter fly mode and use the mouse to look around + WASD and EQ(up/down) to move
- Hold MMB and move the mouse to rotate around the center. 
- Hold shift + MMB to pan the camera
- Scrollwheel to zoom in/out

---

### Building
Builds using CMake Tools and Clang as compiler in VSCode on Windows.\
Requires Crypto++ and glfw libraries in /lib/ (Optionally adjust CMake file to link them from elsewhere).\
Clangd is setup for formatting (using clang-format) aswell as Autocomplete, Warnings etc.\
Building from source requires you to provide your own Api Access through a "secrets.h" file in src/Spotify/. This file should define the following variables:
```cpp
static const std::string clientID = "...";
//base64 contains base64_encode(clientID+":"+clientSecret)
static const std::string base64 = "...";
static const std::string redirectURL = "..."; //eg: "https://olesenjonas.github.io/"
static const std::string encodedRedirectURL = "..."; //eg: "https%3A%2F%2Folesenjonas.github.io%2F"
```
Client Secret and ID can be retrieved after registering an application at https://developer.spotify.com/dashboard/applications

#### Cross-Platform

Windows only code is used for two cases:
- string <-> wstring conversion using MultiByteToWideChar
- Opening URL using ShellExecute

Creates #error if \_WIN32 is not defined. Everything else should work cross-platform.

### WIP

These are things I think are useful but are not implemented yet (and may never be):\
(In no particular order)
- Error Handling (Spoiler: theres currently almost none, but really should be)\
  Especially for:
  - access-token timeout, which would be as simple as refreshing the token after 1h\
    thats how long they last currently, enough for me
  - api requests limit
- Fuzzy-String comparisons for searching
- Filtering based on Genre
- Resize "Split" between both Tables
- Loading more Font ranges (and a Font that supports them) (Arabic, Chinese, etc.)
- Make loading the covers safer (currently pretty unsafe with .death()-ed threads, no guarantee they finish
- Hide individual columns of the tables
- Use comma to search for multiple strings
- Better (looking) UI
