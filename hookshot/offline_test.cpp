// Offline tests of the built DLL, no game needed (sh hookshot/build.sh settings-test | menu-preview).
//   offline_test.exe settings <dll> [file ...]   defaults and each settings file through the reader and the writer
//   offline_test.exe menu <dll> <out dir> [advanced 0|1] [w] [h] [scale]   the menu's tabs as PNGs (CPU painted)
// Linux (sh build.sh linux-settings-test): the settings part, the .so through dlopen (the menu preview is Windows only)
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <cstdio>
typedef void* HMODULE;
inline HMODULE LoadLibraryA(const char* p) { return dlopen(p, RTLD_NOW | RTLD_LOCAL); }
inline void* GetProcAddress(HMODULE m, const char* n) { return dlsym(m, n); }
inline unsigned long GetLastError() { const char* e = dlerror(); if (e) printf("%s\n", e); return 0; }
#endif
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

typedef int (*RoundTripFn)(const char*, char*, int);
typedef int (*PreviewFn)(int, int, int, float, int, unsigned char*);

static uint32_t Crc(const unsigned char* p, size_t n) {
    static uint32_t tab[256];
    if (!tab[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t v = i; for (int k = 0; k < 8; k++) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1; tab[i] = v; }
    uint32_t c = ~0u;
    for (size_t i = 0; i < n; i++) c = tab[(c ^ p[i]) & 255] ^ (c >> 8);
    return ~c;
}
static void Be32(std::vector<unsigned char>& v, uint32_t x) { for (int s = 24; s >= 0; s -= 8) v.push_back(static_cast<unsigned char>(x >> s)); }
static void Chunk(FILE* f, const char* type, const std::vector<unsigned char>& d) {
    std::vector<unsigned char> len; Be32(len, static_cast<uint32_t>(d.size()));
    fwrite(len.data(), 1, 4, f);
    std::vector<unsigned char> b(type, type + 4);
    b.insert(b.end(), d.begin(), d.end());
    fwrite(b.data(), 1, b.size(), f);
    std::vector<unsigned char> crc; Be32(crc, Crc(b.data(), b.size()));
    fwrite(crc.data(), 1, 4, f);
}
// RGB PNG, deflate "stored" blocks (no compression: no zlib needed)
static bool WritePng(const char* path, const unsigned char* rgba, int w, int h) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    const unsigned char sig[8] = {0x89, 0x50, 0x4e, 0x47, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    std::vector<unsigned char> hd;
    Be32(hd, static_cast<uint32_t>(w)); Be32(hd, static_cast<uint32_t>(h));
    hd.push_back(8); hd.push_back(2); hd.push_back(0); hd.push_back(0); hd.push_back(0);
    Chunk(f, "IHDR", hd);
    std::vector<unsigned char> raw;
    raw.reserve(static_cast<size_t>(h) * (w * 3 + 1));
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        for (int x = 0; x < w; x++) { const unsigned char* p = rgba + (static_cast<size_t>(y) * w + x) * 4; raw.push_back(p[0]); raw.push_back(p[1]); raw.push_back(p[2]); }
    }
    std::vector<unsigned char> z = {0x78, 0x01};
    uint32_t s1 = 1, s2 = 0;
    for (unsigned char c : raw) { s1 = (s1 + c) % 65521; s2 = (s2 + s1) % 65521; }
    for (size_t i = 0; i < raw.size();) {
        size_t n = raw.size() - i > 65535 ? 65535 : raw.size() - i;
        z.push_back(i + n == raw.size() ? 1 : 0);
        z.push_back(static_cast<unsigned char>(n)); z.push_back(static_cast<unsigned char>(n >> 8));
        z.push_back(static_cast<unsigned char>(~n)); z.push_back(static_cast<unsigned char>(~n >> 8));
        z.insert(z.end(), raw.begin() + static_cast<long long>(i), raw.begin() + static_cast<long long>(i + n));
        i += n;
    }
    Be32(z, (s2 << 16) | s1);
    Chunk(f, "IDAT", z);
    Chunk(f, "IEND", {});
    return fclose(f) == 0;
}

static std::string Slurp(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    std::string s; char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n);
    fclose(f);
    return s;
}

// lines of a that differ from the same line of d
static int DiffLines(const char* a, const char* d) {
    int diff = 0;
    while (*a && *d) {
        const char *ae = strchr(a, '\n'), *de = strchr(d, '\n');
        if (!ae || !de) break;
        if (ae - a != de - d || strncmp(a, d, static_cast<size_t>(ae - a))) diff++;
        a = ae + 1; d = de + 1;
    }
    return diff;
}

// pass 1 = text -> values -> text; pass 2 = that text again: must come out the same, with nothing "not understood"
static int Settings(HMODULE m, int argc, char** argv) {
    RoundTripFn fn = reinterpret_cast<RoundTripFn>(reinterpret_cast<void*>(GetProcAddress(m, "HsrdSettingsRoundTrip")));
    if (!fn) { printf("HsrdSettingsRoundTrip not exported\n"); return 255; }
    static char a[16384], b[16384], d[16384];
    int fail = 0;
    if (fn(nullptr, d, sizeof d) != 0) { printf("FAIL defaults: could not write\n"); return 1; }
    printf("defaults: %zu bytes, %d lines\n", strlen(d), static_cast<int>(std::count(d, d + strlen(d), '\n')));
    if (fn(d, a, sizeof a) != 0 || strcmp(a, d)) { printf("FAIL defaults: reading its own text changes it\n"); fail = 1; }
    if (getenv("RD_SHOW")) printf("%s\n", d);
    for (int i = 3; i < argc; i++) {
        std::string in = Slurp(argv[i]);
        if (in.empty()) { printf("FAIL %s: not readable\n", argv[i]); fail = 1; continue; }
        int bad1 = fn(in.c_str(), a, sizeof a), bad2 = fn(a, b, sizeof b);
        bool same = !strcmp(a, b);
        printf("%s: %d line(s) not understood, %d line(s) differ from the defaults, second pass %s\n", argv[i], bad1, DiffLines(a, d),
               same && !bad2 ? "identical" : "DIFFERS");
        // a file may name its expected counts in its first line: "# expect: bad N changed M"
        int wantBad = 0, wantDiff = -1;
        sscanf(in.c_str(), "# expect: bad %d changed %d", &wantBad, &wantDiff);
        if (bad1 != wantBad || bad2 != 0 || !same || (wantDiff >= 0 && DiffLines(a, d) != wantDiff)) { printf("FAIL %s\n", argv[i]); fail = 1; }
        if (getenv("RD_SHOW")) printf("%s\n", a);
    }
    printf(fail ? "settings test: FAILED\n" : "settings test: ok\n");
    return fail;
}

static int Menu(HMODULE m, int argc, char** argv) {
    if (argc < 4) return 255;
    RoundTripFn rt = reinterpret_cast<RoundTripFn>(reinterpret_cast<void*>(GetProcAddress(m, "HsrdSettingsRoundTrip")));
    PreviewFn fn = reinterpret_cast<PreviewFn>(reinterpret_cast<void*>(GetProcAddress(m, "HsrdMenuPreview")));
    if (!fn || !rt) { printf("HsrdMenuPreview not exported\n"); return 255; }
    int adv = argc > 4 ? atoi(argv[4]) : 0, w = argc > 5 ? atoi(argv[5]) : 1280, h = argc > 6 ? atoi(argv[6]) : 1000;
    float scale = argc > 7 ? static_cast<float>(atof(argv[7])) : 1.5f;
    static char out[16384];
    // the values the menu shows; RD_UNSAVED: a few values changed after that, as the menu shows unsaved changes
    rt(adv ? "version = 3\nadvanced = on\ndebugtab = on\n" : nullptr, out, sizeof out);
    if (getenv("RD_UNSAVED")) {
        typedef void (*PokeFn)();
        if (PokeFn poke = reinterpret_cast<PokeFn>(reinterpret_cast<void*>(GetProcAddress(m, "HsrdPreviewUnsaved")))) poke();
    }
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 4);
    const char* const TAB[9] = {"", "start", "force", "spin", "getup", "camera", "voices", "controls", "debug"};
    int bad = 0;
    for (int t = 1; t <= (adv ? 8 : 7); t++) {
        int tris = fn(t, w, h, scale, 4, px.data());
        char path[600]; snprintf(path, sizeof path, "%s/%s%s.png", argv[3], TAB[t], adv ? "-advanced" : "");
        bool ok = WritePng(path, px.data(), w, h);
        printf("%s: %d triangles%s\n", path, tris, ok ? "" : " (NOT WRITTEN)");
        if (!ok || tris < 100) bad++;
    }
    printf(bad ? "menu preview: FAILED\n" : "menu preview: ok\n");
    return bad;
}

int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: offline_test settings|menu <dll> ...\n"); return 255; }
    HMODULE m = LoadLibraryA(argv[2]);
    if (!m) { printf("could not load %s (error %lu)\n", argv[2], GetLastError()); return 255; }
    return !strcmp(argv[1], "settings") ? Settings(m, argc, argv) : Menu(m, argc, argv);
}
