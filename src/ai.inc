// oot-ragdoll, part of main.cpp: AI thread hooks: start, SlippingDown, get-up warp, stay down

// generic selector update (every AI node, every frame): the part before the original
void OnChoose(uintptr_t node) {
    uintptr_t vptr = Rd<uintptr_t>(node);
    if (vptr == Main() + OFF_REACT_VPTR) {  // runs every frame while he is on his feet
        u64 now = Ticks();
        g_standTick = now;
        if (!g_configLoaded) LoadConfig();
        SaveTick();
        if (g_state != IDLE && now - g_lastActiveTick > TickHz() / 2) { g_state = IDLE; g_warpPending = 0; }  // ragdoll ended elsewhere
        uintptr_t actor = Rd<uintptr_t>(Rd<uintptr_t>(Rd<uintptr_t>(node + 8) + 8) + 0x1f0);
        if (actor) g_player = actor;
        if (g_state == IDLE && g_pressPending) {
            bool fresh = PressFresh();
            Take(&g_pressPending);
            if (fresh && g_cfg.enabled && actor) StartRagdoll(actor);
        }
        VoiceTick();
    } else if (vptr == Main() + OFF_MAIN_VPTR && g_state != IDLE) {
        VoiceTick();  // the Reaction selector does not run while he is down
        SaveTick();
    }
    if (vptr == Main() + OFF_MAIN_VPTR && g_warpPending) {  // get-up: capsule to the body first (AI thread), stand up 2 frames later
        if (g_warpPending == 1 && g_actor) {
            Vec3 p{g_warpPos[0], g_warpPos[1], g_warpPos[2]};
            GCall(Main() + OFF_SET_POS, g_actor, ToGuest(&p, sizeof p), u32(0));
        }
        g_warpPending = g_warpPending + 1;
        if (g_warpPending > 3) { g_warpPending = 0; g_state = IDLE; }
    }
    if (vptr == Main() + OFF_MAIN_VPTR && g_needSlip && g_state == ACTIVE) {
        // SlippingDown right after the start: eyes close, impact dust, floats in water, no player control
        g_needSlip = false;
        GCall(Main() + OFF_SELECT, node, u64(Rd<u8>(node + MAIN_SLIPPINGDOWN)));
    }
}

// SlippingDown ended (he lies still): the selector would pick LDamageStandUp/Swim/Fall. true = nothing new: stay down
// (the hook returns 0 without calling the original).
bool OnSlipNext() {
    if (g_state == ACTIVE) {
        LyingTick();
        if (g_state == ACTIVE) return true;
    }
    return false;
}

// ragdoll controller per-frame update (HugeHit/SlippingDown call it): liveness + get-up press while airborne
void OnRagUpdate(uintptr_t x0) {
    if (g_state != ACTIVE || !g_ctrl || x0 != g_ctrl) return;
    LyingTick();
}
