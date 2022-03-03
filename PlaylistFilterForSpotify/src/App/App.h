#pragma once

#include "CommonStructs/CommonStructs.h"

#include <GLFW/glfw3.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Renderer/Renderer.h"
#include "Spotify/SpotifyApiAccess.h"
#include "Table/Table.hpp"
#include "Track/Track.h"

// todo: move into some InputHandler class
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void resizeCallback(GLFWwindow* window, int w, int h);

class App
{
  public:
    App();
    ~App();

    void run();
    bool pinTrack(Track* track);
    bool startTrackPlayback(const std::string& trackId);
    bool stopPlayback();
    void createPlaylist(const std::vector<Track*>& tracks);
    void extendPinsByRecommendations();

    // todo: make private, add get and/or set

    std::vector<Track> playlist;
    std::vector<Track*> playlistTracks;
    std::vector<Track*> filteredTracks;
    std::vector<Track*> pinnedTracks = {};
    std::unordered_map<std::string, CoverInfo> coverTable;
    // todo: not sure if 100 is enough, change if needed (also need to adjust size in Imgui function)
    std::array<char, 100> stringFilterBuffer{};
    std::array<glm::vec2, Track::featureAmount> featureMinMaxValues;
    Table<TableType::Pinned> pinnedTracksTable;
    Table<TableType::Filtered> filteredTracksTable;

    std::unordered_set<Track*> recommendedTracks;
    bool showRecommendations = false;

    int lastPlayedTrack = -1;
    bool filterDirty = false;
    bool graphingDirty = false;

    int recommendAccuracy = 1;

  private:
    bool shouldClose();

    // todo: this shouldnt instantly get a refresh token
    //  instead it should be an "empty" object until it gets initialized with a userId etc
    SpotifyApiAccess apiAccess;

    Renderer renderer;
};