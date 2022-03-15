#pragma once

#include "CommonStructs/CommonStructs.h"

#include <GLFW/glfw3.h>

#include <future>
#include <optional>
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
    static enum State { LOG_IN, PL_SELECT, MAIN };
    App();
    ~App();

    void run();
    void runLogIn();
    void runPLSelect();
    void runMain();

    void requestAuth();
    bool checkAuth();

    void loadSelectedPlaylist();

    void extractPlaylistIDFromInput();
    std::optional<std::string> checkPlaylistID(std::string_view id);

    void resetFilterValues();
    bool pinTrack(Track* track);
    bool startTrackPlayback(const std::string& trackId);
    bool stopPlayback();
    void createPlaylist(const std::vector<Track*>& tracks);
    void extendPinsByRecommendations();

    const Renderer& getRenderer();

  private:
    // this needs to be first, so it gets initialized first
    Renderer renderer;

  public:
    // todo: make private, add get and/or set
    // this has to hold a potentially huge URL, and dynamically resizing
    // using ImGui Callback from input didnt work (there was a bug somewhere, made request crash)
    std::array<char, 1000> userInput;
    std::string_view playlistID = "";
    std::optional<std::string> playlistStatus;
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

    std::vector<Recommendation> recommendedTracks;
    bool showRecommendations = false;

    int lastPlayedTrack = -1;
    bool filterDirty = false;
    bool graphingDirty = false;

    int recommendAccuracy = 1;

    // App State
    State state = LOG_IN;
    bool userLoggedIn = false;
    bool loadingPlaylist = false;
    float loadPlaylistProgress = 0.0f;
    std::future<void> doneLoading;

  private:
    bool shouldClose();

    // todo: this shouldnt instantly get a refresh token
    //  instead it should be an "empty" object until it gets initialized with a userId etc
    SpotifyApiAccess apiAccess;
};