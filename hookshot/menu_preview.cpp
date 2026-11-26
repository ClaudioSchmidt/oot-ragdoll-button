// Offline menu preview (sh build.sh menu-preview; no game needed): the built DLL draws the menu (src/menu.inc) for a
// few frames in an ImGui context of its own and paints it with the CPU painter (ui_paint.cpp, from Immersive First
// Person). offline_test.cpp loads the DLL and writes one PNG per tab.
#include <windows.h>
#include <cstring>
#include "imgui.h"
#include "imgui_internal.h"   // (HsrdMenuText: the log buffer)
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

// The text of one tab as Dear ImGui logs it (ImGui::LogToBuffer around one frame: every label, shown value, button and
// tick box text, "[x]" / "[ ]" for a tick box), for a before / after inventory of the page (HSML 2.1 Phase 5c,
// work/phase5c). The window is made h tall so no row is scrolled out of view (a row out of view is not drawn, so not
// logged). Call HsrdMenuPreview first (it makes the context). Returns the length, -1 = out too small
extern "C" __declspec(dllexport) int HsrdMenuText(int tab, int w, int h, float scale, char* out, int cap) {
    if (!g_ctx) return -1;
    ImGui::SetCurrentContext(g_ctx);
    ImGuiIO& io = ImGui::GetIO();
    RdMenuSetSize(static_cast<int>(scale * 100 + 0.5f));
    RdMenuStyle(scale, true);
    int len = -1;
    for (int f = 0; f < 5; f++) {
        io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
        io.DeltaTime = 1.f / 60;
        RdMenuFeedPad(io, &g_last, 0, 0, 0);
        if (f == 0) RdMenuGoTab(tab);
        ImGui::NewFrame();
        ImGui::SetWindowSize("###ragdoll", ImVec2(static_cast<float>(w), static_cast<float>(h)));
        if (f == 4) ImGui::LogToBuffer();
        RdBuildMenu();
        if (f == 4) {
            ImGuiContext& g = *ImGui::GetCurrentContext();
            int n = g.LogBuffer.size();
            if (n + 1 <= cap) { memcpy(out, g.LogBuffer.c_str(), static_cast<size_t>(n)); out[n] = 0; len = n; }
            ImGui::LogFinish();
        }
        ImGui::Render();
    }
    return len;
}
