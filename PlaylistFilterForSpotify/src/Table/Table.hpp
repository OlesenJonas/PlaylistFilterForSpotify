#pragma once

#include <algorithm>
#include <numeric>

#include "Track/Track.h"
#include "imgui/imgui.h"

class App;

template <TableType type>
class TableAttributes
{
};

template <>
class TableAttributes<TableType::Filtered>
{
  protected:
    // todo: not sure if static constexpr is correct/needed here
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

  private:
    App& app;
    std::vector<Track*>& tracks;

    const float coverSize = 40.0f;
    const ImVec2 rowSize{0.0f, coverSize};

    static constexpr ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                             ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                             ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable |
                                             ImGuiTableFlags_NoSavedSettings;
    static constexpr ImGuiTableColumnFlags defaultColumnFlag =
        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize;
    static constexpr ImGuiTableColumnFlags noSortColumnFlag =
        defaultColumnFlag | ImGuiTableColumnFlags_NoSort;

    std::vector<ColumnHeader> columnHeaders;
    float tableWidth = 0.f;

    int columnToSortBy = 0;
    bool sortAscending = false;

    inline ImVec2 getTableHeight()
    {
        if constexpr(type == TableType::Pinned)
        {
            return ImVec2(0, rowSize.y * (std::min<int>(tracks.size(), 3) + 0.7f));
        }
        else
        {
            return ImVec2(0, rowSize.y * 14);
        }
    }
};

#include "App/App.h"

template <TableType type>
Table<type>::Table(App& p_app, std::vector<Track*>& p_tracks) : app(p_app), tracks(p_tracks)
{
    columnHeaders = {
        {"#", defaultColumnFlag, 40.f},
        {"", noSortColumnFlag, 40.f},
        {"Name", noSortColumnFlag, 60.f},
        {"Artist(s)", noSortColumnFlag, 60.f}};
    for(const std::string_view& name : Track::FeatureNames)
    {
        columnHeaders.push_back({name.data(), defaultColumnFlag, 60.0f});
    }
}

// todo: move into constructor once the renderer constructor works correctly
template <TableType type>
void Table<type>::calcHeaderWidth()
{
    const int pad = 10;
    for(auto i = 4; i < columnHeaders.size(); i++)
    {
        // For the audio features we can assume that the header will be wieder than any value
        auto& header = columnHeaders[i];
        header.width = pad + ImGui::CalcTextSize(header.name.c_str()).x;
    }
    // spotify gives some guidelines for length of names etc:
    // https://developer.spotify.com/documentation/general/design-and-branding/
    ImVec2 size;
    // Track name: 23 characters
    size = ImGui::CalcTextSize(u8"MMMMMMMMMMMMMMMMMMMMMMM");
    columnHeaders[2].width = std::max(columnHeaders[2].width, size.x);
    // Playlist/album name: 25 characters
    size = ImGui::CalcTextSize(u8"MMMMMMMMMMMMMMMMMMMMMMMMM");
    columnHeaders[2].width = std::max(columnHeaders[2].width, size.x);
    // Artist name: 18 characters
    size = ImGui::CalcTextSize(u8"MMMMMMMMMMMMMMMMMM");
    columnHeaders[3].width = std::max(columnHeaders[2].width, size.x);
    // could optionally limit width to some max value
    //  float maxNameLength = 250;
    //  columnHeaders[2].width = std::min(columnHeaders[2].width, maxNameLength);
    //  columnHeaders[3].width = std::min(columnHeaders[3].width, maxNameLength);
    //  Add additional padding to last column (compensate for scrollbar)
    columnHeaders[columnHeaders.size() - 1].width += 15.f;
    // not sure what ive used tableWidth for, but I think it can be removed by now. Dont see any other
    // references
    //  auto lambda = [](float f, const ColumnHeader& b) -> float
    //  {
    //  return f + b.width;
    //  };
    //  tableWidth = std::accumulate(columnHeaders.begin(), columnHeaders.end(), 0.0f, lambda);
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
    const ImVec2 outer_size = getTableHeight();
    if(ImGui::BeginTable(this->name, 14, flags, outer_size))
    {
        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        for(const auto& header : columnHeaders)
        {
            ImGui::TableSetupColumn(header.name.c_str(), header.flags, header.width);
        }
        ImGui::TableSetupColumn(this->lastColumnId, noSortColumnFlag, 50.f);
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
                constexpr auto hiddenPlayButtonId = hiddenPlayButtonIdGenerator();
                if(ImGui::InvisibleButton(hiddenPlayButtonId, ImVec2(availReg.x, rowSize.y)))
                {
                    app.startTrackPlayback(tracks[row]->id);
                    app.lastPlayedTrack = tracks[row]->index;
                }
                ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + rowTextOffset));
                if(ImGui::IsItemHovered())
                {
                    ImGui::Text(u8"▶");
                }
                else
                {
                    ImGui::Text("%d", tracks[row]->index + 1);
                }
                // ImGui::SetCursorScreenPos(cursorPos);

                ImGui::TableSetColumnIndex(1);
                ImGui::Image((void*)(intptr_t)(tracks[row]->coverInfoPtr->id), ImVec2(coverSize, coverSize));

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%s", tracks[row]->trackNameEncoded.c_str());
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
                ImGui::Text("%s", tracks[row]->albumNameEncoded.c_str());
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", tracks[row]->artistsNamesEncoded.c_str());

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
        ImGui::EndTable();
        if(removeAfterFrame != -1)
        {
            tracks.erase(tracks.begin() + removeAfterFrame);
            removeAfterFrame = -1;
        }
    }
}