// ui_paint: see ui_paint.hpp.
#include "ui_paint.hpp"
#include "imgui.h"
#include <cmath>
#include <cstring>

namespace uipaint {
namespace {

struct Clip { int x0, y0, x1, y1; };   // x1 / y1 exclusive

// dst over an opaque image: s = colour (alpha ignored), a = 0..255
inline uint32_t Blend(uint32_t d, uint32_t s, uint32_t a) {
    uint32_t ia = 255 - a;
    uint32_t rb = (s & 0xff00ff) * a + (d & 0xff00ff) * ia + 0x800080;
    rb = ((rb + ((rb >> 8) & 0xff00ff)) >> 8) & 0xff00ff;
    uint32_t g = (s & 0x00ff00) * a + (d & 0x00ff00) * ia + 0x008000;
    g = ((g + ((g >> 8) & 0x00ff00)) >> 8) & 0x00ff00;
    return rb | g | 0xff000000u;
}

inline int RoundPx(float v) { return static_cast<int>(floorf(v + 0.5f)); }   // first pixel whose centre is at or past v

void FillQuad(const Target& t, const Clip& c, float fx0, float fy0, float fx1, float fy1, uint32_t col) {
    uint32_t a = col >> 24;
    if (!a) return;
    int x0 = RoundPx(fx0), y0 = RoundPx(fy0), x1 = RoundPx(fx1), y1 = RoundPx(fy1);
    if (x0 < c.x0) x0 = c.x0;
    if (y0 < c.y0) y0 = c.y0;
    if (x1 > c.x1) x1 = c.x1;
    if (y1 > c.y1) y1 = c.y1;
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t solid = col | 0xff000000u;
    for (int y = y0; y < y1; y++) {
        uint32_t* p = t.px + static_cast<size_t>(y) * t.stride + x0;
        int n = x1 - x0;
        if (a == 255) for (int i = 0; i < n; i++) p[i] = solid;
        else for (int i = 0; i < n; i++) p[i] = Blend(p[i], col, a);
    }
}

// the atlas' alpha at texel coordinates (in 1/256 texels, already shifted by half a texel), bilinear
inline uint32_t Sample(const Atlas& at, int u8, int v8) {
    int x = u8 >> 8, y = v8 >> 8;
    uint32_t fx = static_cast<uint32_t>(u8 & 255), fy = static_cast<uint32_t>(v8 & 255);
    int x1 = x + 1, y1 = y + 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x >= at.w) x = at.w - 1;
    if (y >= at.h) y = at.h - 1;
    if (x1 >= at.w) x1 = at.w - 1;
    if (y1 >= at.h) y1 = at.h - 1;
    const unsigned char* r0 = at.a8 + static_cast<size_t>(y) * at.w;
    const unsigned char* r1 = at.a8 + static_cast<size_t>(y1) * at.w;
    uint32_t top = r0[x] * (256 - fx) + r0[x1] * fx, bot = r1[x] * (256 - fx) + r1[x1] * fx;
    return (top * (256 - fy) + bot * fy + 32768) >> 16;
}

// an axis-aligned quad showing a piece of the atlas (a glyph): colour * the atlas' alpha
void GlyphQuad(const Target& t, const Clip& c, const Atlas& at, float fx0, float fy0, float fx1, float fy1,
               float u0, float v0, float u1, float v1, uint32_t col) {
    uint32_t ca = col >> 24;
    if (!ca || fx1 <= fx0 || fy1 <= fy0) return;
    int x0 = RoundPx(fx0), y0 = RoundPx(fy0), x1 = RoundPx(fx1), y1 = RoundPx(fy1);
    if (x0 < c.x0) x0 = c.x0;
    if (y0 < c.y0) y0 = c.y0;
    if (x1 > c.x1) x1 = c.x1;
    if (y1 > c.y1) y1 = c.y1;
    if (x0 >= x1 || y0 >= y1) return;
    // texel = (uv * size) at the pixel's centre, minus half a texel for the bilinear taps; 24.8 fixed point
    float du = (u1 - u0) * static_cast<float>(at.w) / (fx1 - fx0), dv = (v1 - v0) * static_cast<float>(at.h) / (fy1 - fy0);
    float tu0 = u0 * static_cast<float>(at.w) + (static_cast<float>(x0) + 0.5f - fx0) * du - 0.5f;
    float tv0 = v0 * static_cast<float>(at.h) + (static_cast<float>(y0) + 0.5f - fy0) * dv - 0.5f;
    int iu0 = static_cast<int>(floorf(tu0 * 256.f)), idu = static_cast<int>(floorf(du * 256.f + 0.5f));
    int iv = static_cast<int>(floorf(tv0 * 256.f)), idv = static_cast<int>(floorf(dv * 256.f + 0.5f));
    bool exact = idu == 256 && idv == 256 && (iu0 & 255) == 0 && (iv & 255) == 0;   // 1 texel = 1 pixel: no filter
    for (int y = y0; y < y1; y++, iv += idv) {
        uint32_t* p = t.px + static_cast<size_t>(y) * t.stride + x0;
        int n = x1 - x0;
        if (exact) {
            int ty = iv >> 8;
            if (ty < 0 || ty >= at.h) continue;
            const unsigned char* row = at.a8 + static_cast<size_t>(ty) * at.w;
            int tx = iu0 >> 8;
            for (int i = 0; i < n; i++, tx++) {
                uint32_t ta = tx >= 0 && tx < at.w ? row[tx] : 0;
                if (!ta) continue;
                uint32_t a = (ta * ca + 127) / 255;
                p[i] = a >= 255 ? (col | 0xff000000u) : Blend(p[i], col, a);
            }
        } else {
            int iu = iu0;
            for (int i = 0; i < n; i++, iu += idu) {
                uint32_t ta = Sample(at, iu, iv);
                if (!ta) continue;
                uint32_t a = (ta * ca + 127) / 255;
                p[i] = a >= 255 ? (col | 0xff000000u) : Blend(p[i], col, a);
            }
        }
    }
}

struct Edge { float A, B, C; };   // A * x + B * y + C >= 0 inside (x, y: pixel centres)
inline Edge MakeEdge(const ImVec2& a, const ImVec2& b) {
    Edge e;
    e.A = -(b.y - a.y); e.B = b.x - a.x;
    e.C = -(e.A * a.x + e.B * a.y);
    return e;
}

// any triangle: one span per row (left and top edges belong to it, right and bottom ones don't, so triangles that
// share an edge blend each pixel once); colour interpolated when the corners differ, the atlas sampled when they
// carry texture coordinates
void Triangle(const Target& t, const Clip& c, const Atlas& at, const ImDrawVert& va, const ImDrawVert& vb0, const ImDrawVert& vc0) {
    const ImDrawVert* pb = &vb0; const ImDrawVert* pc = &vc0;
    float area = (pb->pos.x - va.pos.x) * (pc->pos.y - va.pos.y) - (pb->pos.y - va.pos.y) * (pc->pos.x - va.pos.x);
    if (area == 0) return;
    if (area < 0) { const ImDrawVert* s = pb; pb = pc; pc = s; area = -area; }
    const ImDrawVert& vb = *pb; const ImDrawVert& vc = *pc;
    if (!((va.col | vb.col | vc.col) >> 24)) return;
    float minY = fminf(va.pos.y, fminf(vb.pos.y, vc.pos.y)), maxY = fmaxf(va.pos.y, fmaxf(vb.pos.y, vc.pos.y));
    int y0 = RoundPx(minY), y1 = RoundPx(maxY);
    if (y0 < c.y0) y0 = c.y0;
    if (y1 > c.y1) y1 = c.y1;
    if (y0 >= y1) return;
    Edge e[3] = {MakeEdge(vb.pos, vc.pos), MakeEdge(vc.pos, va.pos), MakeEdge(va.pos, vb.pos)};   // weights of a, b, c
    bool flat = va.col == vb.col && va.col == vc.col;
    bool tex = !(va.uv.x == at.whiteU && va.uv.y == at.whiteV && vb.uv.x == at.whiteU && vb.uv.y == at.whiteV &&
                 vc.uv.x == at.whiteU && vc.uv.y == at.whiteV);
    float inv = 1.f / area;
    for (int y = y0; y < y1; y++) {
        float yc = static_cast<float>(y) + 0.5f;
        int lo = c.x0, hi = c.x1;
        bool out = false;
        for (int k = 0; k < 3 && !out; k++) {
            float v = e[k].B * yc + e[k].C;
            if (e[k].A > 0) { int x = static_cast<int>(ceilf(-v / e[k].A - 0.5f)); if (x > lo) lo = x; }
            else if (e[k].A < 0) { int x = static_cast<int>(ceilf(-v / e[k].A - 0.5f)); if (x < hi) hi = x; }
            else if (v < 0 || (v == 0 && e[k].B < 0)) out = true;
        }
        if (out || lo >= hi) continue;
        uint32_t* p = t.px + static_cast<size_t>(y) * t.stride;
        if (flat && !tex) {
            uint32_t col = va.col, a = col >> 24, solid = col | 0xff000000u;
            if (a == 255) for (int x = lo; x < hi; x++) p[x] = solid;
            else for (int x = lo; x < hi; x++) p[x] = Blend(p[x], col, a);
            continue;
        }
        float xc = static_cast<float>(lo) + 0.5f;
        float w0 = (e[0].A * xc + e[0].B * yc + e[0].C) * inv, w1 = (e[1].A * xc + e[1].B * yc + e[1].C) * inv;
        float d0 = e[0].A * inv, d1 = e[1].A * inv;
        for (int x = lo; x < hi; x++, w0 += d0, w1 += d1) {
            float w2 = 1.f - w0 - w1;
            uint32_t col = va.col;
            if (!flat) {
                float ch[4];
                for (int k = 0; k < 4; k++) {
                    float v = w0 * static_cast<float>((va.col >> (8 * k)) & 255) + w1 * static_cast<float>((vb.col >> (8 * k)) & 255) +
                              w2 * static_cast<float>((vc.col >> (8 * k)) & 255);
                    ch[k] = v < 0 ? 0 : v > 255 ? 255 : v;
                }
                col = static_cast<uint32_t>(ch[0] + 0.5f) | (static_cast<uint32_t>(ch[1] + 0.5f) << 8) |
                      (static_cast<uint32_t>(ch[2] + 0.5f) << 16) | (static_cast<uint32_t>(ch[3] + 0.5f) << 24);
            }
            uint32_t a = col >> 24;
            if (tex) {
                float u = w0 * va.uv.x + w1 * vb.uv.x + w2 * vc.uv.x, v = w0 * va.uv.y + w1 * vb.uv.y + w2 * vc.uv.y;
                uint32_t ta = Sample(at, static_cast<int>(floorf((u * static_cast<float>(at.w) - 0.5f) * 256.f)),
                                     static_cast<int>(floorf((v * static_cast<float>(at.h) - 0.5f) * 256.f)));
                a = (a * ta + 127) / 255;
            }
            if (!a) continue;
            p[x] = a >= 255 ? (col | 0xff000000u) : Blend(p[x], col, a);
        }
    }
}

}  // namespace

void Paint(const ImDrawData* dd, const Target& t, const Atlas& at, uint32_t clear, Stats* st) {
    if (st) *st = Stats{};
    for (int y = 0; y < t.h; y++) {
        uint32_t* p = t.px + static_cast<size_t>(y) * t.stride;
        for (int x = 0; x < t.w; x++) p[x] = clear | 0xff000000u;
    }
    if (!dd || !at.a8) return;
    const ImVec2 off = dd->DisplayPos;
    for (int n = 0; n < dd->CmdListsCount; n++) {
        const ImDrawList* dl = dd->CmdLists[n];
        const ImDrawVert* vtx = dl->VtxBuffer.Data;
        const ImDrawIdx* idx = dl->IdxBuffer.Data;
        for (int ci = 0; ci < dl->CmdBuffer.Size; ci++) {
            const ImDrawCmd& cmd = dl->CmdBuffer[ci];
            if (cmd.UserCallback) continue;
            Clip c = {static_cast<int>(floorf(cmd.ClipRect.x - off.x)), static_cast<int>(floorf(cmd.ClipRect.y - off.y)),
                      static_cast<int>(ceilf(cmd.ClipRect.z - off.x)), static_cast<int>(ceilf(cmd.ClipRect.w - off.y))};
            if (c.x0 < 0) c.x0 = 0;
            if (c.y0 < 0) c.y0 = 0;
            if (c.x1 > t.w) c.x1 = t.w;
            if (c.y1 > t.h) c.y1 = t.h;
            if (c.x0 >= c.x1 || c.y0 >= c.y1) continue;
            const ImDrawIdx* ib = idx + cmd.IdxOffset;
            const ImDrawVert* vb = vtx + cmd.VtxOffset;
            unsigned cnt = cmd.ElemCount;
            for (unsigned i = 0; i + 3 <= cnt;) {
                // two triangles (0 1 2) (0 2 3) over an axis-aligned rectangle of one colour: ImGui's PrimRect / PrimRectUV
                if (i + 6 <= cnt && ib[i + 3] == ib[i] && ib[i + 4] == ib[i + 2]) {
                    const ImDrawVert& a = vb[ib[i]]; const ImDrawVert& b = vb[ib[i + 1]];
                    const ImDrawVert& cc = vb[ib[i + 2]]; const ImDrawVert& d = vb[ib[i + 5]];
                    if (a.col == b.col && a.col == cc.col && a.col == d.col && a.pos.y == b.pos.y && cc.pos.y == d.pos.y &&
                        a.pos.x == d.pos.x && b.pos.x == cc.pos.x && a.pos.x <= b.pos.x && a.pos.y <= cc.pos.y &&
                        a.uv.y == b.uv.y && cc.uv.y == d.uv.y && a.uv.x == d.uv.x && b.uv.x == cc.uv.x) {
                        bool white = a.uv.x == at.whiteU && a.uv.y == at.whiteV && cc.uv.x == at.whiteU && cc.uv.y == at.whiteV;
                        if (white) {
                            FillQuad(t, c, a.pos.x - off.x, a.pos.y - off.y, cc.pos.x - off.x, cc.pos.y - off.y, a.col);
                            if (st) st->quads++;
                        } else {
                            GlyphQuad(t, c, at, a.pos.x - off.x, a.pos.y - off.y, cc.pos.x - off.x, cc.pos.y - off.y,
                                      a.uv.x, a.uv.y, cc.uv.x, cc.uv.y, a.col);
                            if (st) st->glyphs++;
                        }
                        i += 6;
                        continue;
                    }
                }
                ImDrawVert a = vb[ib[i]], b = vb[ib[i + 1]], cc = vb[ib[i + 2]];
                a.pos.x -= off.x; a.pos.y -= off.y; b.pos.x -= off.x; b.pos.y -= off.y; cc.pos.x -= off.x; cc.pos.y -= off.y;
                Triangle(t, c, at, a, b, cc);
                if (st) st->tris++;
                i += 3;
            }
        }
    }
}

uint64_t Hash(const ImDrawData* dd) {
    uint64_t h = 0xcbf29ce484222325ull;
    auto mix = [&h](const void* data, size_t bytes) {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        size_t i = 0;
        for (; i + 8 <= bytes; i += 8) { uint64_t w; memcpy(&w, p + i, 8); h = (h ^ w) * 0x9e3779b97f4a7c15ull; h ^= h >> 29; }
        for (; i < bytes; i++) h = (h ^ p[i]) * 0x100000001b3ull;
    };
    if (!dd) return h;
    mix(&dd->DisplayPos, sizeof dd->DisplayPos);
    mix(&dd->DisplaySize, sizeof dd->DisplaySize);
    for (int n = 0; n < dd->CmdListsCount; n++) {
        const ImDrawList* dl = dd->CmdLists[n];
        mix(dl->VtxBuffer.Data, static_cast<size_t>(dl->VtxBuffer.Size) * sizeof(ImDrawVert));
        mix(dl->IdxBuffer.Data, static_cast<size_t>(dl->IdxBuffer.Size) * sizeof(ImDrawIdx));
        for (int ci = 0; ci < dl->CmdBuffer.Size; ci++) {
            const ImDrawCmd& cmd = dl->CmdBuffer[ci];
            mix(&cmd.ClipRect, sizeof cmd.ClipRect);
            unsigned v[3] = {cmd.VtxOffset, cmd.IdxOffset, cmd.ElemCount};
            mix(v, sizeof v);
        }
    }
    return h;
}

}  // namespace uipaint
