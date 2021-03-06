#pragma once

#include <algorithm>
#include <numeric>

#include "Track/Track.h"
#include "imgui/imgui.h"
#include "utils/imgui_extensions.h"

class App;

template <TableType type>
class TableAttributes
{
};

template <>
class TableAttributes<TableType::Filtered>
{
  protected:
    static constexpr char* name = "Filtered Playlist Tracks";
    static constexpr char* lastColumnId = "##pin";
};

template <>
class TableAttributes<TableType::Pinned>
{
  protected:
    static constexpr char* name = "Pinned Tracks";
    static constexpr char* lastColumnId = "##unpin";
};

template <TableType type>
class Table : TableAttributes<type>
{
  public:
    Table(App& p_app, std::vector<Track*>& p_tracks);
    Table(const Table&&) = delete;            // prevents rvalue binding
    Table(const Table&) = delete;             // copy constr
    Table& operator=(const Table&) = delete;  // copy assign
    Table(Table&&) = delete;                  // move constr
    Table& operator=(Table&& other) = delete; // move assign
                                              // ~Table();
    void draw();
    void calcHeaderWidth();
    void sortData();

    float width = 0.f;

  private:
    App& app;
    std::vector<Track*>& tracks;

    float coverSize = 40.0f;
    ImVec2 rowSize{0.0f, coverSize};

    static constexpr ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                             ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                             ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable |
                                             ImGuiTableFlags_NoSavedSettings;
    static constexpr ImGuiTableColumnFlags defaultColumnFlag =
        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize;
    static constexpr ImGuiTableColumnFlags noSortColumnFlag =
        defaultColumnFlag | ImGuiTableColumnFlags_NoSort;

    std::vector<ColumnHeader> columnHeaders;

    int columnToSortBy = 0;
    bool sortAscending = false;

    inline ImVec2 getTableSize()
    {
        if constexpr(type == TableType::Pinned)
        {
            return ImVec2(width, rowSize.y * (std::min<int>(tracks.size(), 3) + 0.7f));
        }
        else
        {
            return ImVec2(width, rowSize.y * 14);
        }
    }
};

#include "App/App.h"

template <TableType type>
Table<type>::Table(App& p_app, std::vector<Track*>& p_tracks) : app(p_app), tracks(p_tracks)
{
    calcHeaderWidth();
}

// todo: move into constructor once the renderer constructor works correctly
// exist yet, move after refactored
template <TableType type>
void Table<type>::calcHeaderWidth()
{
    const Renderer& renderer = app.getRenderer();
    coverSize = renderer.scaleByDPI(coverSize);
    rowSize = {0.0f, coverSize};

    columnHeaders = {
        {"#", defaultColumnFlag, coverSize},
        {"", noSortColumnFlag, coverSize},
        {"Name", noSortColumnFlag, 0},
        {"Artist(s)", noSortColumnFlag, 0}};
    for(const std::string_view& name : Track::FeatureNames)
    {
        columnHeaders.push_back({name.data(), defaultColumnFlag, 0});
    }
    columnHeaders.push_back({"Genres", noSortColumnFlag, 0});
    const int pad = app.getRenderer().scaleByDPI(10.0f);
    for(auto i = 4; i < columnHeaders.size(); i++)
    {
        // For the audio features we can assume that the header will be wieder than any value
        auto& header = columnHeaders[i];
        header.width = pad + ImGui::CalcTextSize(header.name.c_str()).x;
    }
    // spotify gives some guidelines for length of names etc:
    // https://developer.spotify.com/documentation/general/design-and-branding/
    ImVec2 size;
    // Track name: 23 characters, Playlist/Album name: 25 characters
    // currently combined into one field, so use longer one
    size = ImGui::CalcTextSize(u8"MMMMMMMMMMMMMMMMMMMMMMMMM");
    columnHeaders[2].width = std::max(columnHeaders[2].width, size.x);
    // Artist name: 18 characters
    size = ImGui::CalcTextSize(u8"MMMMMMMMMMMMMMMMMM");
    columnHeaders[3].width = std::max(columnHeaders[2].width, size.x);

    width = ImGui::GetStyle().ChildBorderSize;
    for(const auto& header : columnHeaders)
    {
        width += header.width;
        width += 2 * ImGui::GetStyle().CellPadding.x;
        width += ImGui::GetStyle().ChildBorderSize;
    }

    // hardcode add width for the last "empty" column (no header just contains the (un-)pin buttons)
    //  todo: should really be part of the columnHeaders array
    //  50.0f is the currently used hardcoded value for the columnwidth
    width += renderer.scaleByDPI(50.0f);
    width += 2 * ImGui::GetStyle().CellPadding.x;
    width += ImGui::GetStyle().ChildBorderSize;
};

template <TableType type>
void Table<type>::sortData()
{
    if(tracks.size() > 1) // only need to sort if more than 1 track
    {
        if(sortAscending)
        {
            std::sort(tracks.begin(), tracks.end(), TrackSorter{columnToSortBy});
        }
        else
        {
            std::sort(tracks.rbegin(), tracks.rend(), TrackSorter{columnToSortBy});
        }
    }
}

template <TableType type>
void Table<type>::draw()
{
    if constexpr(type == TableType::Pinned)
    {
        ImGui::Text("Pinned Tracks: %d", static_cast<int>(tracks.size()));
    }
    else if constexpr(type == TableType::Filtered)
    {
        ImGui::Text("Total: %d", static_cast<int>(tracks.size()));
    }
    const ImVec2 outer_size = getTableSize();
    if(ImGui::BeginTable(this->name, columnHeaders.size() + 1, flags, outer_size))
    {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        for(const auto& header : columnHeaders)
        {
            ImGui::TableSetupColumn(header.name.c_str(), header.flags, header.width);
        }
        ImGui::TableSetupColumn(this->lastColumnId, noSortColumnFlag, app.getRenderer().scaleByDPI(50.f));
        ImGui::TableHeadersRow();

        // Sort our data if sort specs have been changed!
        if(ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
        {
            if(sorts_specs->SpecsDirty)
            {
                const auto& sortSpecObj = sorts_specs->Specs[0];
                columnToSortBy = sortSpecObj.ColumnIndex;
                sortAscending = sortSpecObj.SortDirection == ImGuiSortDirection_Ascending;
                sortData();
                sorts_specs->SpecsDirty = false;
            }
        }

        // Demonstrate using clipper for large vertical lists
        ImGuiListClipper clipper;
        clipper.Begin(tracks.size());
        int removeAfterFrame = -1;
        while(clipper.Step())
        {
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                ImGui::TableNextRow();
                ImGui::PushID(row);

                if(tracks[row]->index == app.lastPlayedTrack)
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(122, 122, 122, 80));
                }

                float rowTextOffset = (rowSize.y - ImGui::GetTextLineHeightWithSpacing()) * 0.5f;

                ImGui::TableSetColumnIndex(0);
                ImVec2 availReg = ImGui::GetContentRegionAvail();
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                constexpr auto hiddenPlayButtonIdGenerator = []() -> const char*
                {
                    if constexpr(type == TableType::Pinned)
                    {
                        return "hiddenPlayButton##pinned";
                    }
                    else
                    {
                        return "hiddenPlayButton##filtered";
                    }
                    return "";
                };
                ImGui::Text("%d", tracks[row]->index + 1);

                ImGui::TableSetColumnIndex(1);
                constexpr auto hiddenPlayButtonId = hiddenPlayButtonIdGenerator();
                if(ImGui::ImageHoverButton(
                       hiddenPlayButtonId,
                       reinterpret_cast<ImTextureID>(tracks[row]->coverInfoPtr->id),
                       reinterpret_cast<ImTextureID>(app.getRenderer().spotifyIconHandle),
                       coverSize,
                       0.5f))
                {
                    app.startTrackPlayback(tracks[row]->id);
                    app.lastPlayedTrack = tracks[row]->index;
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(tracks[row]->trackNameEncoded.c_str());
                if(ImGui::IsItemClicked())
                {
                    app.getRenderer().selectedTrack = tracks[row];
                }
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
                ImGui::TextUnformatted(tracks[row]->albumNameEncoded.c_str());
                if(ImGui::IsItemClicked())
                {
                    app.getRenderer().selectedTrack = tracks[row];
                }
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(tracks[row]->artistsNamesEncoded.c_str());

                for(int i = 4; i < 11; i++)
                {
                    ImGui::TableSetColumnIndex(i);
                    ImGui::Text("%.3f", tracks[row]->features[i - 4]);
                }

                ImGui::TableSetColumnIndex(11);
                ImGui::Text("%.0f", std::round(tracks[row]->features[7]));

                ImGui::TableSetColumnIndex(12);
                ImGui::Text("%.3f", tracks[row]->features[8]);

                ImGui::TableSetColumnIndex(13);
                if(ImGui::BeginCombo("##trackGenreCombo", "", ImGuiComboFlags_NoPreview))
                {
                    DynBitset temp = tracks[row]->genreMask;
                    while(temp)
                    {
                        // todo: kind of inefficient, because the DynBitset -> Bool conversion already
                        // iterates through all elements in bitset
                        // so chcking as while-condition while also clearing all bits is super redundant
                        uint32_t smallestIndex = temp.getFirstBitSet();
                        temp.clearBit(smallestIndex);
                        const bool isSelected = app.genreMask.getBit(smallestIndex);
                        if(ImGui::Selectable(app.genres[smallestIndex].c_str(), isSelected))
                        {
                            app.genreMask.toggleBit(smallestIndex);
                            app.filterDirty = true;
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::TableSetColumnIndex(14);
                if constexpr(type == TableType::Pinned)
                {
                    if(ImGui::Button("Unpin"))
                    {
                        removeAfterFrame = row;
                    }
                }
                else
                {
                    if(ImGui::Button("Pin"))
                    {
                        if(app.pinTrack(tracks[row]))
                        {
                            app.lastPlayedTrack = tracks[row]->index;
                        }
                    }
                }

                ImGui::PopID();
            }
        }

        if constexpr(type == TableType::Pinned)
        {
            // From: imgui_demo.cpp
            int hovered_column = -1;
            for(int column = 4; column < 14; column++)
            {
                ImGui::PushID(column);
                if(ImGui::TableGetColumnFlags(column) & ImGuiTableColumnFlags_IsHovered)
                {
                    hovered_column = column;
                }
                if(hovered_column == column && !ImGui::IsAnyItemHovered() && ImGui::IsMouseReleased(1) &&
                   !tracks.empty())
                {
                    ImGui::OpenPopup("ColumnFilterPopup");
                }
                if(ImGui::BeginPopup("ColumnFilterPopup"))
                {
                    if(ImGui::Button("Create Filter##perColumn"))
                    {
                        int featureIndex = column - 4;
                        app.featureMinMaxValues[featureIndex] =
                            glm::vec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::min());
                        for(const Track* trackPtr : app.pinnedTracks)
                        {
                            app.featureMinMaxValues[featureIndex].x = std::min(
                                app.featureMinMaxValues[featureIndex].x, trackPtr->features[featureIndex]);
                            app.featureMinMaxValues[featureIndex].y = std::max(
                                app.featureMinMaxValues[featureIndex].y, trackPtr->features[featureIndex]);
                        }
                        app.filterDirty = true;
                    }
                    ImGui::Text(" based on values in column %s", columnHeaders[column].name.c_str());
                    if(ImGui::Button("Close"))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
        }

        ImGui::EndTable();
        if(removeAfterFrame != -1)
        {
            tracks.erase(tracks.begin() + removeAfterFrame);
            removeAfterFrame = -1;
        }
    }
}