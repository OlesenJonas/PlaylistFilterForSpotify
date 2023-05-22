#pragma once

#include <glad/glad.h>

#include <ImGui/imgui.h>
#include <array>
#include <glm/ext.hpp>
#include <string>
#include <vector>

struct Track;

struct GraphingBufferElement
{
    // position of track
    glm::vec3 p;
    // index of album cover in cover array
    GLuint layer;
    // index in the original track vector. Needed for selection, when raycasting against elements in the track
    // buffer
    GLuint originalIndex;
};

struct ColumnHeader
{
    std::string name;
    ImGuiTableColumnFlags flags;
    float width;
};

struct CoverInfo
{
    std::string url;
    // Layer index in the big cover array
    GLuint layer = 0;
    // OpenGL handle of a texture view, covering just that single layer
    GLuint id = 0xFFFFFFFF;
};

struct TextureLoadInfo
{
    int x;
    int y;
    unsigned char* data;
    CoverInfo* ptr;
};

enum TableType
{
    Pinned,
    Filtered
};

struct Recommendation
{
    Track* track = nullptr;
    mutable uint8_t occurances = 1;

    inline bool operator==(const Recommendation& rhs) const
    {
        return track == rhs.track;
    }
    inline bool operator<(const Recommendation& rhs) const
    {
        return occurances < rhs.occurances;
    }
};
struct RecommendationHash
{
    std::size_t operator()(const Recommendation& r) const
    {
        return std::hash<Track*>{}(r.track);
    }
};

struct StringRefWapperHash
{
    std::size_t operator()(const std::string& obj) const
    {
        std::hash<const std::string*> theHash{};
        return theHash(&obj);
    }
};

// using
struct StringHash
{
    using hashType = std::hash<std::string_view>;
    using is_transparent = void;

    std::size_t operator()(const char* str) const
    {
        return hashType{}(str);
    }
    std::size_t operator()(std::string_view str) const
    {
        // assert(str.size() == 22);
        return hashType{}(str);
    }
    std::size_t operator()(const std::string& str) const
    {
        // assert(str.size() == 22);
        return hashType{}(str);
    }
    template <size_t N>
    std::size_t operator()(const std::array<char, N>& array) const
    {
        return hashType{}(std::string_view{array.data(), N});
    }
};