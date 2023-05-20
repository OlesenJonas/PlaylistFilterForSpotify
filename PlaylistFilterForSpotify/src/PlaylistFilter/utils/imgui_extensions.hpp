#pragma once

#include <ImGui/imgui.h>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
    #define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <ImGui/imgui_internal.h>

// TODO: not sure why it wrote it like this, update and see if anything breaks
#define IMGUI_ACTIVATE(X, B)                                                                                      \
    if(X)                                                                                                         \
    {                                                                                                             \
        B = true;                                                                                                 \
    }

namespace ImGui
{
    // size_arg (for each axis) < 0.0f: align to end, 0.0f: auto, > 0.0f: specified size
    void
    HorizontalBar(float fraction_start, float fraction_end, const ImVec2& size_arg, const char* overlay = nullptr);

    int PlotLines2D(
        const char* label,
        float* xValues,
        float* yValues,
        int count,
        const char* overlay_text,
        ImVec2 xRange,
        ImVec2 yRange,
        ImVec2 graphSize,
        ImU32 color = GetColorU32(ImGuiCol_PlotLines));

    int PlotLines2D(
        const char* label,
        float** xValues,
        float** yValues,
        int* count,
        int datasets,
        const char* overlay_text,
        ImVec2 xRange,
        ImVec2 yRange,
        ImVec2 graphSize,
        ImU32* colors = nullptr);

    int resizeUserInputVector(ImGuiInputTextCallbackData* data);

    bool ImageHoverButton(
        const char* str_id, ImTextureID defaultTex, ImTextureID hoverTex, float size, float hoverScale = 1.0f);

    // from imgui_demo.cpp
    void HelpMarker(const char* desc);
    void HelpMarkerFromLastItem(const char* desc);

}; // namespace ImGui