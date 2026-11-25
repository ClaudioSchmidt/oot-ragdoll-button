// Platform layer: main.cpp's game logic runs on the Switch (exlaunch, Eden) and in Hookshot (HSML mod DLL).
// Guest addresses stay guest addresses everywhere (uintptr_t); host access only through:
//   Main()                  guest address of main's start
//   P<T>(guest) / Rd<T>()   host pointer to guest memory (0 stays nullptr) / read
//   GCall(fn, args...)      call a game function at a guest address, integer / pointer-sized args only, result x0
//   GCallV3(fn, args...)    same, result Vec3 (s0-s2)
//   ToGuest / FromGuest     local buffers for pointer args (Switch: the buffer itself; Hookshot: copied to guest scratch)
//   Ticks / TickHz, RD_LOG, MemBarrier, CfgLoad / CfgSave (settings file)
//   MenuOpen / MenuTakesPad / MenuToggle / MenuPad / MenuFrame / MenuMouseBlock   the settings menu's host: a shared
//                           menu the mod's page is registered in (Hookshot: HSML's, hookshot/hs_rt.cpp; Switch: the Mod
//                           Menu, hooks_switch.inc)
#pragma once

struct Vec3 { float x, y, z; };

#ifdef HOOKSHOT
#include "../hookshot/platform_hs.hpp"
#else
#include "lib.hpp"
#include "nn/fs.hpp"
#include <cstdio>

namespace nn::fs {
    Result MountSdCard(char const* mount);
    Result SetFileSize(FileHandle handle, s64 size);
}

inline uintptr_t Main() { return exl::util::modules::GetTargetStart(); }
template<typename T> T* P(uintptr_t a) { return reinterpret_cast<T*>(a); }
template<typename T> T Rd(uintptr_t a) { return *P<T>(a); }
template<typename... A> u64 GCall(uintptr_t fn, A... a) { return reinterpret_cast<u64 (*)(A...)>(fn)(a...); }
template<typename... A> Vec3 GCallV3(uintptr_t fn, A... a) { return reinterpret_cast<Vec3 (*)(A...)>(fn)(a...); }
inline uintptr_t ToGuest(const void* p, size_t) { return reinterpret_cast<uintptr_t>(p); }
inline void FromGuest(void*, uintptr_t, size_t) {}
inline u64 Ticks() { u64 t; asm volatile("mrs %0, cntpct_el0" : "=r"(t)); return t; }
inline u64 TickHz() { return 19200000; }
inline void MemBarrier() { asm volatile("dmb ish" ::: "memory"); }
#define RD_LOG(...) Logging.Log(__VA_ARGS__)

// the settings menu: a page in the Mod Menu every Switch mod carries (hsml nx/modmenu_nx.inc, included by
// hooks_switch.inc: the module with the newest menu hosts it; this mod's page is registered there as on PC). When this
// module hosts, the menu gets its pads and its frame from the npad hook (input.inc)
#include "hsml_menu.hpp"
#include "nx_fs.hpp"
#include <atomic>
void HsmlNxPad(u64 buttons, float lx, float ly);
void HsmlNxFrame();
inline bool MenuOpen() { return HsmlMenuIsOpen(); }
inline bool MenuTakesPad() { return HsmlMenuTakesPad(); }
inline void MenuToggle() { HsmlMenuOpen(); }   // this mod's shortcut: the menu at its page, closed when it shows it
inline void MenuPad(u64 b, float lx, float ly) { HsmlNxPad(b, lx, ly); }
inline void MenuFrame() { HsmlNxFrame(); }
inline int MenuMouseBlock() { return 0; }

// settings file on the SD card (ragdollsd:/OoT-Mods/oot_ragdoll.txt). No file call that can fail (hsml nx/nx_fs.hpp:
// one that fails while a module starts ends the game): a file or folder is looked up in its folder's listing first
#define RD_CONFIG_DIR "ragdollsd:/OoT-Mods"
#define RD_CONFIG_PATH "ragdollsd:/OoT-Mods/oot_ragdoll.txt"
// the SD card, mounted on the first call (the settings on the AI thread, or the menu's own settings on the pad's)
inline bool CfgSdOk() {
    static std::atomic_flag busy = ATOMIC_FLAG_INIT;
    static std::atomic<int> state{0};   // 1 mounted, 2 not available
    if (int s = state.load()) return s == 1;
    while (busy.test_and_set(std::memory_order_acquire)) asm volatile("yield");
    if (!state.load()) state = nn::fs::MountSdCard("ragdollsd") == 0 || nn::fs::MountSdCardForDebug("ragdollsd") == 0 ? 1 : 2;
    busy.clear(std::memory_order_release);
    return state.load() == 1;
}
// text of the file into buf (NUL-terminated, at most cap-1 bytes); false = defaults stay (logged)
inline bool CfgLoad(char* buf, size_t cap, size_t* len) {
    if (!CfgSdOk()) {
        RD_LOG("[oot_ragdoll] SD not available, defaults kept");
        return false;
    }
    long size = hsnx::ReadText(RD_CONFIG_PATH, buf, cap);
    if (size < 0) {
        RD_LOG("[oot_ragdoll] oot_ragdoll.txt not found, defaults kept");
        return false;
    }
    *len = static_cast<size_t>(size);
    return true;
}
inline bool CfgSave(const char* buf, size_t len) {
    if (!CfgSdOk()) return false;
    hsnx::MakeDir(RD_CONFIG_DIR);
    if (!hsnx::WriteAt(RD_CONFIG_PATH, buf, len)) {
        RD_LOG("[oot_ragdoll] could not write oot_ragdoll.txt");
        return false;
    }
    return true;
}
#endif
