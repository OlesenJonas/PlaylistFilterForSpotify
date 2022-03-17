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

// todo: move into some InputHandler file
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
    void pinTracks(const std::vector<Track*>& tracks);
    bool startTrackPlayback(const std::string& trackId);
    bool stopPlayback();
    void createPlaylist(const std::vector<Track*>& tracks);
    void extendPinsByRecommendations();

    const Renderer& getRenderer();

  private:
    // this needs to be first, so it gets initialized first
    Renderer renderer;

  public:
    enum State
    {
        LOG_IN,
        PL_SELECT,
        MAIN
    };
    // todo: make private, add get and/or set
    // this has to hold a potentially huge URL, and dynamically resizing
    // using ImGui Callback from input didnt work (there was a bug somewhere, made request crash)
    std::array<char, 1000> userInput;

    // Info about the loaded playlist
    std::string_view playlistID = "";
    std::optional<std::string> playlistStatus;
    std::vector<Track> playlist;
    std::vector<Track*> playlistTracks;
    std::unordered_map<std::string, CoverInfo> coverTable;

    // Variables for working with the playlist
    std::array<char, 100> stringFilterBuffer{};
    std::array<glm::vec2, Track::featureAmount> featureMinMaxValues;
    std::vector<Track*> pinnedTracks = {};
    Table<TableType::Pinned> pinnedTracksTable;
    std::vector<Track*> filteredTracks;
    Table<TableType::Filtered> filteredTracksTable;
    int recommendAccuracy = 1;
    std::vector<Recommendation> recommendedTracks;
    bool showRecommendations = false;
    bool filterDirty = false;

    // Rendering related app state
    bool graphingDirty = false;
    int lastPlayedTrack = -1;
    bool showDeviceErrorWindow = false;

    // App State
    State state = LOG_IN;
    bool userLoggedIn = false;
    bool loadingPlaylist = false;
    float loadPlaylistProgress = 0.0f;
    std::future<void> doneLoading;

  private:
    bool shouldClose();

    SpotifyApiAccess apiAccess;
};