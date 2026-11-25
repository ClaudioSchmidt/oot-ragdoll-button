// menu_host: what every host of the menu (src/menu.inc) does the same way: ImGui's style for it and the game pad as
// ImGui's navigation input. Used by the offline preview (menu_preview.cpp); in the game HSML's menu does both. From Immersive
// First Person's src/menu_host.hpp.
#pragma once
#include "imgui.h"

// scale: 1.5 = the menu's size on a 1080-row picture. painted: the CPU painter draws it onto an opaque panel (no
// see-through window, no textured lines)
inline void RdMenuStyle(float scale, bool painted) {
    ImGuiStyle& st = ImGui::GetStyle();
    st = ImGuiStyle();
    ImGui::StyleColorsDark();
    st.ScaleAllSizes(scale);
    ImGui::GetIO().FontGlobalScale = scale;
    if (painted) {
        st.Colors[ImGuiCol_WindowBg].w = 1.f;
        st.Colors[ImGuiCol_PopupBg].w = 1.f;
        st.AntiAliasedLinesUseTex = false;
        st.WindowRounding = 0;
    }
}

// The pads' buttons (npad bits) and left stick as ImGui's gamepad keys. Switch layout: A (right) activates, B (bottom)
// cancels. last: the caller's copy of the buttons fed before
inline void RdMenuFeedPad(ImGuiIO& io, unsigned long long* last, unsigned long long b, float lx, float ly) {
    static const struct { int bit; ImGuiKey k; } map[] = {
        {0, ImGuiKey_GamepadFaceDown}, {1, ImGuiKey_GamepadFaceRight}, {2, ImGuiKey_GamepadFaceUp}, {3, ImGuiKey_GamepadFaceLeft},
        {6, ImGuiKey_GamepadL1}, {7, ImGuiKey_GamepadR1}, {8, ImGuiKey_GamepadL2}, {9, ImGuiKey_GamepadR2},
        {10, ImGuiKey_GamepadStart}, {11, ImGuiKey_GamepadBack},
        {12, ImGuiKey_GamepadDpadLeft}, {13, ImGuiKey_GamepadDpadUp}, {14, ImGuiKey_GamepadDpadRight}, {15, ImGuiKey_GamepadDpadDown}};
    for (auto& m : map) {
        bool now = (b >> m.bit) & 1;
        if (now != (((*last >> m.bit) & 1) != 0)) io.AddKeyEvent(m.k, now);
    }
    *last = b;
    auto ax = [&](ImGuiKey neg, ImGuiKey pos, float v) {
        const float dz = 0.25f;
        io.AddKeyAnalogEvent(neg, v < -dz, v < -dz ? (-v - dz) / (1 - dz) : 0);
        io.AddKeyAnalogEvent(pos, v > dz, v > dz ? (v - dz) / (1 - dz) : 0);
    };
    ax(ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, lx);
    ax(ImGuiKey_GamepadLStickDown, ImGuiKey_GamepadLStickUp, ly);
}
