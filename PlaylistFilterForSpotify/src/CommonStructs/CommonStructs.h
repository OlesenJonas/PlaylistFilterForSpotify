#pragma once

#include <glad/glad.h>

#include "imgui/imgui.h"
#include <glm/ext.hpp>
#include <string>

struct TrackBufferElement
{
    // position of track
    glm::vec3 p;
    // index of album cover in cover array
    GLuint layer;
    // need this index for selection, when raycasting against elements in this buffer
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
    GLuint layer = 0;
    GLuint id;
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