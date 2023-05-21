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
    App();
    ~App();

    void run();
    Renderer& getRenderer();
    std::unordered_map<std::string, CoverInfo>& getCoverTable();
    void setSelectedTrack(Track* track);
    void toggleWindowVisibility();
    int getLastPlayedTrackIndex();
    bool startTrackPlayback(Track* track);

    // returns true, if the genre with given passes the current filter
    bool genrePassesFilter(uint32_t index);
    void addGenreToFilter(uint32_t index);
    void toggleGenreFilter(uint32_t index);
    const char* getGenreName(uint32_t index);

    bool pinTrack(Track* track);
    void pinTracks(const std::vector<Track*>& tracks);

    void setFeatureFiltersFromPins(int featureIndex);

    Track*
    raycastAgainstGraphingBuffer(glm::vec3 rayPos, glm::vec3 rayDir, glm::vec3 worldCamX, glm::vec3 worldCamY);

  private:
    enum State
    {
        LOG_IN,
        PLAYLIST_SELECT,
        PLAYLIST_LOAD,
        MAIN
    };

    void runLogIn();
    void createLogInUI();

    void runPlaylistSelect();
    void createPlaylistSelectUI();

    void runPlaylistLoad();
    void createPlaylistLoadUI();

    void runMain();
    void createMainUI();

    // ---

    bool shouldClose();

    // Opens a webpage to request authorization from spotify
    void requestAuth();
    // Check if the given redirect URL contains a valid authorization code
    bool checkAuth();
    /*
        Attempt to extract a PlaylistID from the user input field.
        Sets this class' playlistID field (either to the ID, or the empty string)
    */
    void extractPlaylistIDFromInput();
    // Load all data relevant for analyzing the given playlist from Spotify
    void loadSelectedPlaylist();

    std::optional<std::string> checkPlaylistID(std::string_view id);

    void resetFeatureFilters();
    void refreshFilteredTracks();

    void extendPinsByRecommendations();

    void createPlaylist(const std::vector<Track*>& tracks);

    void generateGraphingData();

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
    FilteredTracksTable filteredTracksTable;

    // Pin related variables
    std::vector<Track*> pinnedTracks;
    PinnedTracksTable pinnedTracksTable;
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
};