// ui_paint: paints Dear ImGui's draw data on the CPU into an RGBA8 image. No platform code: the Switch overlay
// (overlay_nx.inc) paints the menu with it, and the offline preview (sh hookshot/build.sh menu-preview) writes PNGs.
// What ImGui emits and how it is painted:
//   axis-aligned quads of one colour (frames, backgrounds, 1 px lines)  -> row fills / blends
//   axis-aligned textured quads (text glyphs, the mouse cursor)         -> the font atlas' alpha, bilinear
//   every other triangle (rounded corners, anti-aliased edges, ticks)   -> spans per row, colour interpolated
// The image is opaque: the result's alpha byte is always 255 (the screen copy takes no alpha).
#pragma once
#include <cstdint>

struct ImDrawData;

namespace uipaint {

struct Target { uint32_t* px; int w, h, stride; };   // RGBA8, R in the low byte (ImGui's IM_COL32); stride in pixels
struct Atlas { const unsigned char* a8; int w, h; float whiteU, whiteV; };   // font atlas as 8-bit alpha; its white pixel
struct Stats { int quads, glyphs, tris; };

// Clears the image to `clear` and paints every draw list. Positions are ImGui's (DisplayPos = the image's top left).
void Paint(const ImDrawData* dd, const Target& t, const Atlas& a, uint32_t clear, Stats* st = nullptr);
// A hash of everything Paint would use: equal hash = the same picture, nothing to repaint.
uint64_t Hash(const ImDrawData* dd);

}  // namespace uipaint
