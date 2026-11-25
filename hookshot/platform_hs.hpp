// Hookshot platform (see src/platform.hpp, which defines Vec3 before including this): guest memory at
// host = guest + 0x100000000000, main at guest 0x80020000. Built on the HSML mod runtime
// (../../hsml/sdk/hsml_mod.hpp: P, Rd, Main, Ctx, CallGuest, ToGuest, FromGuest, Ticks, HsLog).
// No windows.h here (main.cpp has names like near). Guest calls and ToGuest work only inside an InHook scope
// (hs_rt.cpp opens one around every On* call).
#pragma once
#include "hsml_mod.hpp"
#include "hsml_menu.hpp"

inline u64 TickHz() { static const u64 f = TickFreq(); return f; }
inline void MemBarrier() { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
#define RD_LOG(...) HsLog(__VA_ARGS__)

// the sdk's CallGuest returns GuestRet; main.cpp wants the Switch shapes
template<typename... A> u64 GCall(uintptr_t fn, A... a) { return CallGuest(fn, a...).x0; }
template<typename... A> Vec3 GCallV3(uintptr_t fn, A... a) { GuestRet r = CallGuest(fn, a...); return Vec3{r.s[0], r.s[1], r.s[2]}; }

// settings file <modDir>\oot_ragdoll.txt (hs_rt.cpp)
bool CfgLoad(char* buf, size_t cap, size_t* len);
bool CfgSave(const char* buf, size_t len);

// the menu: HSML's shared one (hsml_menu.hpp; hs_rt.cpp registers src/menu.inc's page there). HSML owns the window,
// the pads while it is open (it clears the states before this mod's hook sees them) and the mouse over it
inline bool MenuOpen() { return HsmlMenuIsOpen(); }
inline bool MenuTakesPad() { return HsmlMenuTakesPad(); }
inline void MenuToggle() { HsmlMenuOpen(); }   // this mod's shortcut: the menu at its page, closed when it shows it
inline void MenuPad(u64, float, float) {}
inline void MenuFrame() {}   // (HSML draws the menu; on Switch the hosting mod drives its frame)
inline int MenuMouseBlock() { return 0; }
