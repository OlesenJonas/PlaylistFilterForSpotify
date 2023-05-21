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

class App
{
  public:
    enum State
    {
        LOG_IN,
        PLAYLIST_SELECT,
        PLAYLIST_LOAD,
        MAIN
    };

    App();
    ~App();

    void run();

    // todo: these should be private
    void runLogIn();
    void createLogInUI();

    void runPlaylistSelect();
    void createPlaylistSelectUI();

    void runPlaylistLoad();
    void createPlaylistLoadUI();

    void runMain();
    void createMainUI();

    // ---

    void requestAuth();
    bool checkAuth();

    void loadSelectedPlaylist();

    void extractPlaylistIDFromInput();
    std::optional<std::string> checkPlaylistID(std::string_view id);

    void resetFilterValues();
    void refreshFilteredTracks();
    bool pinTrack(Track* track);
    void pinTracks(const std::vector<Track*>& tracks);
    bool startTrackPlayback(const std::string& trackId);
    void createPlaylist(const std::vector<Track*>& tracks);
    void extendPinsByRecommendations();

    void generateGraphingData();

    Renderer& getRenderer();

    // this needs to be first, so it gets initialized first
    //  (Table initialization needs ImGui calc width)
    Renderer renderer;
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

    // Filtering related variables
    DynBitset currentGenreMask;
    ImGuiTextFilter genreFilter;
    ImGuiTextFilter nameFilter;
    std::array<glm::vec2, Track::featureAmount> featureMinMaxValues;
    bool filterDirty = false;
    std::vector<Track*> filteredTracks;
    Table<TableType::Filtered> filteredTracksTable;

    // Pin related variables
    std::vector<Track*> pinnedTracks;
    Table<TableType::Pinned> pinnedTracksTable;
    int recommendAccuracy = 1;
    std::vector<Recommendation> recommendedTracks;
    bool showRecommendations = false;

    // Rendering related app state
    bool uiHidden = false;
    int graphingFeatureX = 0;
    int graphingFeatureY = 1;
    int graphingFeatureZ = 2;
    float coverSize3D = 0.1f;
    bool graphingDirty = false;
    int lastPlayedTrack = -1;
    bool showDeviceErrorWindow = false;
    Track* selectedTrack = nullptr;
    std::vector<GraphingBufferElement> graphingData;

    // App State
    State state = LOG_IN;
    float loadPlaylistProgress = 0.0f;
    std::future<void> doneLoading;
    bool canLoadCovers = true;
    int coversTotal;
    int coversLoaded;

  private:
    bool shouldClose();
};