#pragma once
#include "Table.hpp"
#include "ImGui/imgui.h"
#include "Table/Table.hpp"
#include "Track/Track.hpp"
#include <App/App.hpp>

PinnedTracksTable::PinnedTracksTable(App& p_app, std::vector<Track*>& p_tracks) : Table(p_app, p_tracks)
{
    tableName = "Pinned Tracks";
    lastColumnButtonName = "Unpin all";
    hiddenPlayButtonID = "hiddenPlayButton##pinned";
}

FilteredTracksTable::FilteredTracksTable(App& p_app, std::vector<Track*>& p_tracks) : Table(p_app, p_tracks)
{
    tableName = "Filtered Tracks";
    lastColumnButtonName = "Pin all";
    hiddenPlayButtonID = "hiddenPlayButton##filtered";
}

void PinnedTracksTable::lastColumnButton(int row, float buttonWidth, int* flag)
{
    if(ImGui::Button("Unpin", ImVec2(buttonWidth, 0)))
    {
        *flag = row;
    }
}
void FilteredTracksTable::lastColumnButton(int row, float buttonWidth, int* flag)
{
    (void)flag;
    if(ImGui::Button("Pin", ImVec2(buttonWidth, 0)))
    {
        app.pinTrack(tracks[row]);
    }
}

void PinnedTracksTable::lastColumnHeaderButtonAction()
{
    app.clearPinsAfterFrame = true;
}

void FilteredTracksTable::lastColumnHeaderButtonAction()
{
    app.pinAllAfterFrame = true;
}

Table::Table(App& p_app, std::vector<Track*>& p_tracks) : app(p_app), tracks(p_tracks)
{
    calcHeaderWidth();
}

void Table::calcHeaderWidth()
{
    const Renderer& renderer = app.getRenderer();
    coverSize = renderer.scaleByDPI(coverSize);
    rowSize = {0.0f, coverSize};

    columnHeaders = {
        {"#", ImGuiTableColumnFlags_NoHide, coverSize},
        {"Cover",
         ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize |
             ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoHeaderLabel,
         coverSize},
        {"Name", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoHide, 0},
        {"Artist(s)", ImGuiTableColumnFlags_NoSort, 0}};
    for(const std::string_view& name : Track::FeatureNames)
    {
        columnHeaders.push_back({name.data(), 0, 0});
    }
    columnHeaders[columnHeaders.size() - 1].flags |= ImGuiTableColumnFlags_DefaultHide;
    columnHeaders.push_back({"Genres", ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_NoResize, 0});
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
    size = ImGui::CalcTextSize("MMMMMMMMMMMMMMMMMMMMMMMMM");
    columnHeaders[2].width = std::max(columnHeaders[2].width, size.x);
    // Artist name: 18 characters
    size = ImGui::CalcTextSize("MMMMMMMMMMMMMMMMMM");
    columnHeaders[3].width = std::max(columnHeaders[2].width, size.x);
};

void Table::sortData()
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

void Table::draw(float height, bool updateColumnsState, bool stateToSet)
{
    assert(strcmp(tableName, "") != 0);
    assert(strcmp(lastColumnButtonName, "") != 0);

    // larger of the two, just so its consistent
    float unpinAllWidth = ImGui::CalcTextSize("Unpin all").x;
    float lastColumnNameWidth = ImGui::CalcTextSize(lastColumnButtonName).x;
    float lastColumnWidth = unpinAllWidth;
    // static bool firstFrame = true;
    // static float unpinButtonWidth = 0.0f;
    // if(firstFrame)
    // {
    //     ImGui::Button("Unpin");
    //     unpinButtonWidth = ImGui::GetItemRectSize().x;
    //     firstFrame = false;
    // }

    const ImVec2 outer_size = ImVec2(0, height);
    // +1 because the last column with the pin/unpin buttons isnt part of columnHeaders
    // hardcoded table name, so both tables have same ID -> settings like size, visibility are shared
    if(ImGui::BeginTable("trackTable", columnHeaders.size() + 1, flags, outer_size))
    {
        if(updateColumnsState)
        {
            for(int i = 0; i < Track::featureAmount; i++)
            {
                ImGui::TableSetColumnEnabled(4 + i, stateToSet);
            }
        }

        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        for(const auto& header : columnHeaders)
        {
            ImGui::TableSetupColumn(header.name.c_str(), header.flags, header.width);
        }
        ImGui::TableSetupColumn(
            "Pin/Unpin",
            ImGuiTableColumnFlags_NoSort | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize |
                ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoHeaderLabel,
            lastColumnWidth);

        // ImGui::TableHeadersRow();
        // Setup rows manually (code taken from TableHeadersRow())
        {
            ImGuiContext& g = *GImGui;
            ImGuiTable* table = g.CurrentTable;

            // Layout if not already done (this is automatically done by TableNextRow, we do it here solely to
            // facilitate stepping in debugger as it is frequent to step in TableUpdateLayout)
            if(!table->IsLayoutLocked)
                ImGui::TableUpdateLayout(table);

            // Open row
            const float row_y1 = ImGui::GetCursorScreenPos().y;
            const float row_height = ImGui::TableGetHeaderRowHeight();
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers, row_height);

            const int columns_count = ImGui::TableGetColumnCount();
            for(int column_n = 0; column_n < columns_count; column_n++)
            {
                if(!ImGui::TableSetColumnIndex(column_n))
                    continue;

                // Push an id to allow unnamed labels (generally accidental, but let's behave nicely with them)
                // - in your own code you may omit the PushID/PopID all-together, provided you know they won't
                // collide
                // - table->InstanceCurrent is only >0 when we use multiple BeginTable/EndTable calls with same
                // identifier.
                const char* name = (ImGui::TableGetColumnFlags(column_n) & ImGuiTableColumnFlags_NoHeaderLabel)
                                       ? ""
                                       : ImGui::TableGetColumnName(column_n);
                ImGui::PushID(table->InstanceCurrent * table->ColumnsCount + column_n);
                auto x = ImGui::GetContentRegionAvail().x;
                if(column_n == columns_count - 1)
                {
                    ImGui::Dummy(ImVec2((lastColumnWidth - lastColumnNameWidth) * 0.5f, 0.0f));
                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::Text("%s", lastColumnButtonName);
                    if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
                    {
                        lastColumnHeaderButtonAction();
                    }
                    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
                }
                ImGui::TableHeader(name);
                ImGui::PopID();
            }

            // Allow opening popup from the right-most section after the last column.
            ImVec2 mouse_pos = ImGui::GetMousePos();
            if(ImGui::IsMouseReleased(1) && ImGui::TableGetHoveredColumn() == columns_count)
                if(mouse_pos.y >= row_y1 && mouse_pos.y < row_y1 + row_height)
                    ImGui::TableOpenContextMenu(-1); // Will open a non-column-specific popup.
        }

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

                if(tracks[row]->index == app.getLastPlayedTrackIndex())
                {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(122, 122, 122, 80));
                }

                float rowTextOffset = (rowSize.y - ImGui::GetTextLineHeightWithSpacing()) * 0.5f;

                ImGui::TableSetColumnIndex(0);
                ImVec2 availReg = ImGui::GetContentRegionAvail();
                ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                ImGui::Text("%d", tracks[row]->index + 1);

                ImGui::TableSetColumnIndex(1);
                assert(strcmp(hiddenPlayButtonID, "") != 0);
                if(ImGui::ImageHoverButton(
                       hiddenPlayButtonID,
                       reinterpret_cast<ImTextureID>(tracks[row]->coverInfoPtr->id),
                       reinterpret_cast<ImTextureID>(app.getRenderer().spotifyIconHandle),
                       coverSize,
                       0.5f))
                {
                    app.startTrackPlayback(tracks[row]);
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(tracks[row]->trackNameEncoded.c_str());
                if(ImGui::IsItemClicked())
                {
                    app.setSelectedTrack(tracks[row]);
                }
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 160));
                ImGui::TextUnformatted(tracks[row]->albumNameEncoded.c_str());
                if(ImGui::IsItemClicked())
                {
                    app.setSelectedTrack(tracks[row]);
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
                        const bool isSelected = app.genrePassesFilter(smallestIndex);
                        if(ImGui::Selectable(app.getGenreName(smallestIndex), isSelected))
                        {
                            app.toggleGenreFilter(smallestIndex);
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::TableSetColumnIndex(14);
                lastColumnButton(row, lastColumnWidth, &removeAfterFrame);

                ImGui::PopID();
            }
        }

        additionalLogic();

        ImGui::EndTable();
        if(removeAfterFrame != -1)
        {
            tracks.erase(tracks.begin() + removeAfterFrame);
            removeAfterFrame = -1;
        }
    }
}

void PinnedTracksTable::additionalLogic()
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
        if(hovered_column == column && !ImGui::IsAnyItemHovered() && ImGui::IsMouseReleased(1) && !tracks.empty())
        {
            ImGui::OpenPopup("ColumnFilterPopup");
        }
        if(ImGui::BeginPopup("ColumnFilterPopup"))
        {
            ImGui::Text("%s", columnHeaders[column].name.c_str());
            if(ImGui::Button("Create filter for this feature from pins##perColumn"))
            {
                const int featureIndex = column - 4;
                app.setFeatureFiltersFromPins(featureIndex);
            }
            if(ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}

void FilteredTracksTable::additionalLogic()
{
}