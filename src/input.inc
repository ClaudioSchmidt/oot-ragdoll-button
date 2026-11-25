// oot-ragdoll, part of main.cpp: npad hook: buttons, the settings button gestures, hiding our buttons from the game

// playonly (default): the mod owns its buttons only in play: Link is playable (his Reaction selector ran within the
// last second: it may pause during short actions), ragdolled, or was until a moment ago (he is getting up). In the
// game's menus, cutscenes and other long states the game keeps them. Off = 0.9.0: always.
constexpr u64 GETUP_WINDOW_S = 3;
bool InPlay() {
    if (!g_cfg.playonly) return true;
    u64 now = Ticks();
    return g_state == ACTIVE || now - g_standTick < TickHz() || now - g_lastActiveTick < GETUP_WINDOW_S * TickHz();
}
// playonly: a ragdoll press counts for one second; one made while he gets up counts until he stands (4 s at most).
// So a press in a menu or cutscene starts nothing later, and mashing through the get-up still drops him again.
bool PressFresh() {
    if (!g_cfg.playonly) return true;
    u64 now = Ticks(), t = g_pressTick, la = g_lastActiveTick;
    if (now - t < TickHz()) return true;
    return la && t >= la && t - la < GETUP_WINDOW_S * TickHz() && now - t < 4 * TickHz();
}

// 1.1.1 (user: the controls as HSML's and Immersive Camera's): an action's gesture from "its buttons (all of them) or
// its PC key are held now". Tap: the press; double tap: a second press within the time; hold: held for the time, once
// per hold. (1.1.0's settings button is gone: its double tap is Next force level, the menu has its own binding, a new
// button is set in the menu.)
struct BindRun { bool on, done; u64 since, lastPress; };
bool BindFire(const Bind& b, BindRun& r, bool on, u64 now) {
    bool press = on && !r.on;
    r.on = on;
    if (!on) { r.done = false; return false; }
    if (press) r.since = now;
    u64 t = static_cast<u64>(b.time * static_cast<float>(TickHz()));
    switch (b.trig) {
        case TR_HOLD:
            if (r.done || now - r.since < t) return false;
            r.done = true;
            return true;
        case TR_DOUBLE:
            if (!press) return false;
            if (r.lastPress && now - r.lastPress <= t) { r.lastPress = 0; return true; }
            r.lastPress = now;
            return false;
        default: return press;
    }
}

// A button row records its buttons (the menu's Set; g_bindTarget = the mask it fills): first until nothing is held,
// then every button pressed (the first two) until all are let go; 6 s without a press: nothing changes. true = it
// records (the press is not for the menu or the game)
bool CapFeed(u64 held) {
    static bool armed; static u64 got, last;
    volatile u64* t = g_bindTarget;
    held &= BUTTON_BITS;
    u64 fresh = held & ~last;
    last = held;
    if (!t) { armed = false; got = 0; return false; }
    if (!armed) {
        if (!held) armed = true;
        got = 0;
    } else {
        for (int k = 0; k < 16; k++) {
            u64 bit = 1ull << k;
            if ((fresh & bit) && !(got & bit) && __builtin_popcountll(got) < 2) got |= bit;
        }
        if (!held && got) {
            *t = got;   // (kept by the menu's Save)
            RD_LOG("[oot_ragdoll] button row set: %s", ButtonName(got));
            g_bindTarget = nullptr; g_bindDoneTick = Ticks(); armed = false; got = 0;
            return true;
        }
    }
    if (!got && Ticks() - g_bindStart > 6 * TickHz()) { g_bindTarget = nullptr; armed = false; }
    return true;
}

// npad read loop, after one of its 5 GetNpadStates calls. slot = x22, fp = x29 (frame: [fp-0x18] states, [fp-0xc] count,
// [fp-4] npad id), tag = [fp-0x1c] = which stub was called (OFF_NPAD_STUBS index)
void OnInput(u64 slot, uintptr_t fp, u32 tag) {
    if (slot > 8) return;
    uintptr_t states = Rd<uintptr_t>(fp - 0x18);
    s32 count = Rd<s32>(fp - 0xc);
    if (!states) return;
    if (count > 16) count = 16;
    u64 newest = 0; s64 bestSample = -1; s32 sx = 0, sy = 0;
    for (int i = 0; i < count; i++) {
        uintptr_t s = states + i * 0x28;
        if (Rd<s64>(s) > bestSample) { bestSample = Rd<s64>(s); newest = Rd<u64>(s + 8); sx = Rd<s32>(s + 0x10); sy = Rd<s32>(s + 0x14); }
    }
    if (count > 0 && tag < 5) {  // raw buttons (other mods may have cleared theirs from the states)
        struct { s64 sampling; u64 buttons; s32 stick[4]; u32 attr, pad; } raw[1]{};
        u32 id = Rd<u32>(fp - 4);
        uintptr_t graw = ToGuest(raw, sizeof raw);   // (Hookshot: this runs the stub's detour again, guarded there)
        if (static_cast<s32>(GCall(Main() + OFF_NPAD_STUBS[tag], graw, s32(1), ToGuest(&id, sizeof id))) > 0) {
            FromGuest(raw, graw, sizeof raw);        // the stub wrote the states here
            newest = raw[0].buttons;
        }
    }
    g_slotButtons[slot] = count > 0 ? newest : 0;
    static u64 stickSlot = 99;  // the slot that last moved the stick or pressed something: it also reports the centred stick
    static s32 rawX, rawY;      // its left stick
    if (count > 0 && (sx || sy || newest)) stickSlot = slot;
    if (count > 0 && slot == stickSlot) { rawX = sx; rawY = sy; }
    u64 held = 0;
    for (int i = 0; i < 9; i++) held |= g_slotButtons[i];
    held |= g_keyHeld;   // 1.1.0: the PC keys (Hookshot), as buttons of their own (KB_*)

    // The shared menu (Hookshot: HSML's; Switch: the Mod Menu). A button row that records takes its buttons here
    // whenever this hook has them (Switch always; Hookshot while the game keeps the controller), through RdMenuBindPad
    // while HSML has cleared the states. While the menu has the pad, the game and the ragdoll get nothing.
    MenuPad(held, rawX / 32767.f, rawY / 32767.f);
    MenuFrame();   // Switch, when this module hosts the Mod Menu: its frame (at most 70 a second, whatever the pad reads)
    static bool wasOpen; static u64 swallow;   // buttons held when the menu closed: not for the game until released
    // the menu has the pad: nothing for the game (buttons + both sticks of all 16 states) and nothing for the ragdoll
    auto menuOwnsPad = [&] {
        g_held = held; g_heldPlay = 0; g_stickX = 0; g_stickY = 0; wasOpen = true;
        Take(&g_pressPending); Take(&g_getupPending); g_getupDownTick = 0;
        for (int i = 0; i < 16; i++) memset(P<u8>(states + i * 0x28 + 8), 0, 0x18);
    };
    // a button row records (Hookshot: while HSML has cleared the states, RdMenuBindPad feeds it instead)
#ifdef HOOKSHOT
    bool hostFeeds = MenuTakesPad();
#else
    bool hostFeeds = false;
#endif
    if (MenuOpen() && g_bindTarget && !hostFeeds) CapFeed(held);
    if (MenuOpen() && g_bindTarget) {
        if (MenuTakesPad()) { menuOwnsPad(); return; }
        g_held = held; wasOpen = true;
        return;
    }
    u64 now = Ticks();
    // This menu (its own binding; the shared menu's binding is HSML's): also while the menu is open (Switch: the pad
    // reaches us then)
    static BindRun runMenu;
    if (BindFire(g_cfg.menu, runMenu, BindHeld(g_cfg.menu, held, 0), now)) MenuToggle();
    if (MenuTakesPad()) { menuOwnsPad(); return; }
    if (wasOpen && !MenuOpen()) { wasOpen = false; swallow = held; }
    swallow &= held;
    g_stickX = rawX; g_stickY = rawY; g_heldPlay = held;

    bool play = g_cfg.enabled && InPlay();
    static BindRun runRag, runGetup, runForce;
    if (BindFire(g_cfg.rag, runRag, BindHeld(g_cfg.rag, held & ~swallow, KB_RAG), now) && play) { g_pressTick = now; Bump(&g_pressPending); }
    u64 mask = play ? g_cfg.rag.mask | KB_RAG : 0;
    // get up: the same trigger on GetupMask (A / B when it shares a button with Ragdoll); g_getupDownTick = held since
    // (getup_hold), from a press while he is down
    u64 gu = g_cfg.enabled ? GetupMask() : 0;
    Bind gb = g_cfg.getup; gb.mask = gu;
    bool guOn = g_cfg.enabled && BindHeld(gb, held, KB_GETUP);
    if (g_state == ACTIVE && guOn && !runGetup.on) { u64 t = Ticks(); g_getupDownTick = t ? t : 1; }
    if (BindFire(gb, runGetup, guOn, now) && g_state == ACTIVE) Bump(&g_getupPending);
    if (!guOn) g_getupDownTick = 0;
    gu |= KB_GETUP;
    // next force level (not while the menu has the pad: d-pad presses there move through it)
    if (BindFire(g_cfg.nextforce, runForce, BindHeld(g_cfg.nextforce, held, KB_FORCE), now)) {
        g_cfg.force = (g_cfg.force + 1) % 5; KeepValue(&g_cfg.force); RequestSave(); SayForce(g_cfg.force);
        RD_LOG("[oot_ragdoll] force -> %s", FORCES[g_cfg.force]);
    }
    // hide the get-up button while he is down, and until it is released (no sword swing when he stands)
    static bool hideGetup;
    hideGetup = gu && (g_state == ACTIVE || (hideGetup && (held & gu)));
    if (hideGetup) mask |= gu;
    mask |= swallow;
    // the port puts the mouse into the pad states: over the open menu it must not turn the camera or press buttons
    int mb = MenuOpen() ? MenuMouseBlock() : 0;
    if (MenuOpen()) wasOpen = true;
    g_held = held;
    if (!mask && !mb) return;
    for (int i = 0; i < 16; i++) {  // hide our buttons from the game
        uintptr_t s = states + i * 0x28;
        *P<u64>(s + 8) &= (mb & 2) ? 0 : ~mask;
        if (mb & 1) { *P<s32>(s + 0x18) = 0; *P<s32>(s + 0x1c) = 0; }
    }
}
