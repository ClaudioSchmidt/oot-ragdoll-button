// oot-ragdoll: press the button -> Link goes limp from any pose (keeps his speed + a kick); while ragdolled every press
// is another kick and holding pushes (camera + left stick aim). He stays down until the get-up button (default B),
// then gets up with the game's stand-up animation.
// OoT (0400486024DEC000) v2.0.0, offsets in FINDINGS.md ("Clean ragdoll + stand-up on demand").
// The same file builds for the Switch (exlaunch, hooks_switch.inc) and for Hookshot (-DHOOKSHOT, hookshot/hooks_hs.inc);
// platform.hpp is the only place that knows the difference. Hook bodies are the On* functions.
// Parts (one per subject, in this order):
//   offsets.inc   game addresses, fingerprints of the hook sites
//   config.inc    every value a player can change, and the table that describes them (file keys, menu rows)
//   state.inc     state shared by the hooks, and which thread owns what
//   voice.inc     Link's voices
//   settings.inc  the settings file
//   steer.inc     aim direction, spin, body velocity helpers, "on the ground"
//   ragdoll.inc   start, kicks, get-up
//   input.inc     npad hook: buttons, settings button gestures
//   ai.inc        AI thread hooks
//   physics.inc   physics hooks: start kick, kick / push / spin / roll, impact voice
//   camera.inc    speed camera
//   enemy.inc     enemy ragdolls (test build, Hookshot only: F6 / F7 / F8)
//   menu.inc      the settings menu (RD_MENU: Hookshot; a page in HSML's shared menu, registered in hookshot/hs_rt.cpp)
#include "platform.hpp"
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#ifdef RD_MENU
#include "imgui.h"
#endif

#ifdef HOOKSHOT
bool HsReadable(uintptr_t guest);   // hookshot/hs_rt.cpp: the guest address is committed, readable memory
#endif

namespace {

#include "offsets.inc"
#include "config.inc"
#include "state.inc"
#include "voice.inc"
#include "settings.inc"
#include "steer.inc"
#include "ragdoll.inc"
#include "input.inc"
#include "ai.inc"
#include "physics.inc"
#include "camera.inc"
#include "enemy.inc"

// The hook sites must be the 2.0.0 main (fingerprints); false = the mod stays off. Switch: at start (before the
// settings file is read, so always checked). Hookshot: at the first hook call (guest memory is not populated yet when
// HsmlInit runs); the import stub is not checked there (Hookshot's compiler bypasses the game's import thunks).
bool RagCheck() {
    if (!g_cfg.versioncheck) { RD_LOG("[oot_ragdoll] versioncheck = off: game build not checked"); return true; }
    for (auto& f : FINGERPRINTS) {
#ifdef HOOKSHOT
        if (f.off == OFF_NPAD_STUBS[2]) continue;
#endif
        u32 w = Rd<u32>(Main() + f.off);
        bool hookedByOther = f.word == 0xD503237F && (w >> 26) == 5;  // another mod (e.g. Hero) hooked it: stack on top
        if (w != f.word && !hookedByOther) {
            RD_LOG("[oot_ragdoll] main+0x%lx is %08x, expected %08x: other game version, ragdoll off (false alarm? versioncheck = off in the settings file)", f.off, w, f.word);
            return false;
        }
    }
    return true;
}

}  // namespace

#ifdef RD_MENU
#include "menu.inc"
#endif
#ifdef HOOKSHOT
#include "hooks_hs.inc"
#else
#include "hooks_switch.inc"
#endif
