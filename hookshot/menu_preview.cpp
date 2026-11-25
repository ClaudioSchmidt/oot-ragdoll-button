// Offline menu preview (sh build.sh menu-preview; no game needed): the built DLL draws the menu (src/menu.inc) for a
// few frames in an ImGui context of its own and paints it with the CPU painter (ui_paint.cpp, from Immersive First
// Person). offline_test.cpp loads the DLL and writes one PNG per tab.
#include <windows.h>
#include <cstring>
#include "imgui.h"
#include "menu_host.hpp"
#include "ui_paint.hpp"

void RdBuildMenu();
void RdMenuGoTab(int tab);
void RdMenuSetSize(int percent);

namespace {
ImGuiContext* g_ctx;
unsigned long long g_last;
uipaint::Atlas g_atlas;
}

// tab: 1..8 (Start .. Debug); scale: the Menu size (1.5 = 150 %). rgba: w * h * 4 bytes. Returns the triangles ImGui emitted.
extern "C" __declspec(dllexport) int HsrdMenuPreview(int tab, int w, int h, float scale, int frames, unsigned char* rgba) {
    if (!g_ctx) {
        g_ctx = ImGui::CreateContext();
        ImGui::SetCurrentContext(g_ctx);
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
        unsigned char* px; int tw, th;
        io.Fonts->GetTexDataAsAlpha8(&px, &tw, &th);
        g_atlas = {px, tw, th, io.Fonts->TexUvWhitePixel.x, io.Fonts->TexUvWhitePixel.y};
    }
    ImGui::SetCurrentContext(g_ctx);
    ImGuiIO& io = ImGui::GetIO();
    RdMenuSetSize(static_cast<int>(scale * 100 + 0.5f));
    RdMenuStyle(scale, true);
    for (int f = 0; f < frames; f++) {
        io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
        io.DeltaTime = 1.f / 60;
        RdMenuFeedPad(io, &g_last, 0, 0, 0);
        if (f == 0) RdMenuGoTab(tab);
        ImGui::NewFrame();
        RdBuildMenu();
        ImGui::Render();
    }
    ImDrawData* dd = ImGui::GetDrawData();
    uipaint::Target t = {reinterpret_cast<uint32_t*>(rgba), w, h, w};
    uipaint::Paint(dd, t, g_atlas, 0xff282420u, nullptr);
    return dd->TotalIdxCount / 3;
}
