#pragma once

#include <algorithm>
#include <numeric>

#include <ImGui/imgui.h>
#include <Track/Track.hpp>
#include <utils/imgui_extensions.hpp>

class App;

class Table
{
  public:
    Table() = delete;
    Table(const Table&&) = delete;            // prevents rvalue binding
    Table(const Table&) = delete;             // copy constr
    Table& operator=(const Table&) = delete;  // copy assign
    Table(Table&&) = delete;                  // move constr
    Table& operator=(Table&& other) = delete; // move assign

    void draw(float height, bool updateColumnsState, bool stateToSet);
    void calcHeaderWidth();
    void sortData();

    float width = 0.f;

    float coverSize = 40.0f;
    ImVec2 rowSize{0.0f, coverSize};

  protected:
    Table(App& p_app, std::vector<Track*>& p_tracks);

    App& app;
    std::vector<Track*>& tracks;

    const char* tableName = "";
    const char* hiddenPlayButtonID = "";
    const char* lastColumnButtonName = "";
    virtual void lastColumnButton(int row, float buttonWidth, int* flag) = 0;
    virtual void lastColumnHeaderButtonAction() = 0;
    virtual void additionalLogic() = 0;

    static constexpr ImGuiTableFlags flags =      //
        ImGuiTableFlags_ScrollY                   //
        | ImGuiTableFlags_RowBg                   //
        | ImGuiTableFlags_BordersOuter            //
        | ImGuiTableFlags_BordersV                //
        | ImGuiTableFlags_SizingStretchSame       //
        | ImGuiTableFlags_Resizable               //
        | ImGuiTableFlags_Sortable                //
        | ImGuiTableFlags_NoSavedSettings         //
        | ImGuiTableFlags_AlwaysVerticalScrollbar //
        | ImGuiTableFlags_Hideable;               //

    std::vector<ColumnHeader> columnHeaders;

    int columnToSortBy = 0;
    bool sortAscending = false;

    virtual ImVec2 getTableSize() = 0;
};

// dont even *really* need inheritance here
class PinnedTracksTable : public Table
{
  public:
    PinnedTracksTable(App& p_app, std::vector<Track*>& p_tracks);

  private:
    ImVec2 getTableSize() final;
    void lastColumnButton(int row, float buttonWidth, int* flag) final;
    void lastColumnHeaderButtonAction() final;
    void additionalLogic() final;
};
class FilteredTracksTable : public Table
{
  public:
    FilteredTracksTable(App& p_app, std::vector<Track*>& p_tracks);

  private:
    ImVec2 getTableSize() final;
    void lastColumnButton(int row, float buttonWidth, int* flag) final;
    void lastColumnHeaderButtonAction() final;
    void additionalLogic() final;
};