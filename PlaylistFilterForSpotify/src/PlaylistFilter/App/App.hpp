#pragma once

#include <GLFW/glfw3.h>

#include <future>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <CommonStructs/CommonStructs.hpp>
#include <DynamicBitset/DynamicBitset.hpp>
#include <Renderer/Renderer.hpp>
#include <Spotify/SpotifyApiAccess.hpp>
#include <Table/Table.hpp>
#include <Track/Track.hpp>

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
    void runPLLoad();
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

    Renderer& getRenderer();

  private:
    // this needs to be first, so it gets initialized first
    //  todo: that shouldnt be the case!
    Renderer renderer;

  public:
    enum State
    {
        LOG_IN,
        PL_SELECT,
        PL_LOAD,
        MAIN
    };
    // todo: make private, add get and/or set

    SpotifyApiAccess apiAccess;

    // buffer for all kinds of user input (auth URL among other things, so may need a lot of space)
    std::array<char, 1000> userInput;

    std::string playlistID;
    std::string playlistName;

    /*
        todo: dont like this being a vector, size is determined once and then constant for the rest of the program!
              and it mustnt be resized anyways, since many places reference elements through pointers which need
              to stay valid!
    */
    std::vector<Track> playlist;
    /*
        A filtered playlist is just a vector of pointers to the remaining tracks.
        To make resetting filters faster, playlistTracks is a cached version including all tracks
    */
    std::vector<Track*> playlistTracks;
    /*
        Same goes for this, is initiated once, and mustnt be changed afterwards

        Key string is AlbumID
    */
    std::unordered_map<std::string, CoverInfo> coverTable;

    std::vector<std::string> genreNames;

    // Variables for filtering the playlist
    DynBitset currentGenreMask;
    ImGuiTextFilter nameFilter;
    std::array<glm::vec2, Track::featureAmount> featureMinMaxValues;
    bool filterDirty = false;
    std::vector<Track*> filteredTracks;
    Table<TableType::Filtered> filteredTracksTable;

    // Variables for working with pinned tracks
    std::vector<Track*> pinnedTracks;
    Table<TableType::Pinned> pinnedTracksTable;
    int recommendAccuracy = 1;
    std::vector<Recommendation> recommendedTracks;
    bool showRecommendations = false;

    // Rendering related app state
    bool graphingDirty = false;
    int lastPlayedTrack = -1;
    bool showDeviceErrorWindow = false;

    // App State
    State state = LOG_IN;
    float loadPlaylistProgress = 0.0f;
    std::future<void> doneLoading;

  private:
    bool shouldClose();
};