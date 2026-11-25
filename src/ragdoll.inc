// oot-ragdoll, part of main.cpp: ragdoll start, kicks and get-up while he is down

inline float KickSpeed() { return g_tune.kick[g_cfg.force]; }
inline float PushSpeed() { return g_tune.push[g_cfg.force]; }

bool Pushing() {  // push button held (after the start frame)
    return g_state == ACTIVE && g_firstFrameDone && BindHeld(g_cfg.rag, g_heldPlay, KB_RAG) && PushSpeed() > 0;
}

// get-up now: stand up where the body lies. His capsule (= where he stands up) only chases the ragdoll at ~30 m/s and
// gets stuck on terrain, so it can be far behind (FINDINGS "Snap-back"): from warpGap on it is moved to the body first
// (AI thread, OnChoose), and the stand-up veto lets go 3 AI frames later. The warp makes the body hitch, so not always.
void GetUp(const char* why) {
    g_burstPending = false;
    g_getupTick = Ticks();   // the game's get-up voice in the next seconds: the Getting up list (OnVoiceAs)
    memcpy(g_warpPos, g_bodyPos, sizeof(g_warpPos));
    MemBarrier();
    float gap = 0;
    if (g_actor) for (int i = 0; i < 3; i++) { float d = Rd<float>(g_actor + 0x31c + 4 * i) - g_bodyPos[i]; gap += d * d; }
    if ((g_bodyPos[0] || g_bodyPos[1] || g_bodyPos[2]) && gap > g_tune.warpGap * g_tune.warpGap) { if (!g_warpPending) g_warpPending = 1; }
    else if (!g_warpPending) g_state = IDLE;
    if (g_actor) RD_LOG("[oot_ragdoll] get-up (%s): capsule %.1f %.1f %.1f body %.1f %.1f %.1f at %.1f m/s", why, Rd<float>(g_actor + 0x31c),
        Rd<float>(g_actor + 0x320), Rd<float>(g_actor + 0x324), g_bodyPos[0], g_bodyPos[1], g_bodyPos[2], g_bodySpeed);
}

// while ragdolled (any of the three hooks that prove he still is): a new press = another kick; the get-up button ends
// it (the stand-up veto then lets go).
void LyingTick() {
    u64 now = Ticks();
    g_lastActiveTick = now;
    if (Take(&g_pressPending)) {
        g_getupWantTick = 0;   // a kick takes a waiting get-up back
        if (g_firstFrameDone) {
            g_burstPending = true;
            g_fallSince = 0; g_fallLast = 0;   // a kick starts a new arc: its fall starts over
            // mashing: no voice while one still waits or just played (they queue 0.3 s apart and would trail behind)
            if (g_cfg.kickvoice && !SayBusy() && now - g_lastSayTick > TickHz() / 4) SayWhen(V_PRESS, g_bodySpeed, "kick");
        }
    }
    if (g_warpPending) return;   // already getting up
    if (Take(&g_getupPending)) g_getupWantTick = now ? now : 1;
    u64 want = g_getupWantTick, down = g_getupDownTick;
    bool held = g_tune.getupHold > 0 && down && now - down > static_cast<u64>(g_tune.getupHold * TickHz());
    if (!want && !held) return;
    // still flying / tumbling: wait for him to slow down (getupWait), or for the button to be held long enough
    bool slow = g_bodySpeed <= g_tune.getupSpeed;
    if (!slow && !held) {
        if (now - want > static_cast<u64>(g_tune.getupWait * TickHz())) {
            g_getupWantTick = 0;
            RD_LOG("[oot_ragdoll] get-up dropped: body still at %.1f m/s", g_bodySpeed);
        }
        return;
    }
    // (several hooks may get here in the same frame: only the one that clears the request goes on)
    if (want && !__atomic_compare_exchange_n(const_cast<u64*>(&g_getupWantTick), &want, u64(0), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) return;
    g_getupDownTick = 0;   // (a hold counts once; the input thread starts it again with the next press)
    GetUp(slow ? "press" : "held");
}

// press: start the ragdoll directly (no hit reaction: no scream, no turn), aimed at his movement, Weak list
void StartRagdoll(uintptr_t actor) {
    uintptr_t comp = Rd<uintptr_t>(actor + 0x270);
    if (!comp) return;
    uintptr_t phys = Rd<uintptr_t>(comp + 0x50);
    phys = phys ? Rd<uintptr_t>(phys + 0x20) : 0;
    phys = phys ? Rd<uintptr_t>(phys + 0x10) : 0;
    if (!phys) return;
    uintptr_t ctrl = GCall(Main() + OFF_GET_CTRL, phys);
    if (!ctrl) return;
    g_actor = actor;
    g_facing = atan2f(Rd<float>(actor + 0x330), Rd<float>(actor + 0x348));
    for (int i = 0; i < 3; i++) g_vel[i] = Rd<float>(actor + 0x388 + 4 * i);
    float vh = sqrtf(g_vel[0] * g_vel[0] + g_vel[2] * g_vel[2]);
    // +0x0c aims the fling relative to his facing: moveYaw - facing + pi (pi = forward)
    float angle = vh > 0.3f ? atan2f(g_vel[0], g_vel[2]) - g_facing + 3.14159265f : 3.14159265f;
    struct Request {
        uintptr_t actor; float power, angle, f10, f14, f18; u32 u1c, u20, strength, situation, anim;
        float dir[3]; u32 u3c;
    } req{actor, 1.f, angle, 0.f, 12.f, 6.f, 1, 2, 0 /* Weak */, 0, 0, {0.f, 0.f, -1.f}, 2};
    static_assert(sizeof(Request) == 0x40);
    // per-ragdoll state
    g_bodySpeed = sqrtf(vh * vh + g_vel[1] * g_vel[1]);
    g_bodyPos[0] = g_bodyPos[1] = g_bodyPos[2] = 0; g_warpPending = 0; g_physFrames = 0; g_restFrames = 0;
    g_firstFrameDone = false; g_firstKickOpen = true; g_needSlip = true; g_burstPending = false; g_spinPending = false;
    g_steerFrames = 0; g_sinceSteer = 999; g_lastDrop = 0; g_impactPrevCont = 0;
    g_startWait = 0; g_spineOk = false; g_groundHold = 0; g_groundRun = 0; g_fallSince = 0; g_fallLast = 0;
    bool airStart = fabsf(g_vel[1]) > AIR_START_VY;   // Link falls or jumps: he starts in the air
    g_airFrames = airStart ? AIR_FRAMES : 0;
    Take(&g_getupPending); g_getupWantTick = 0; g_getupDownTick = 0;
    g_ctrl = ctrl; g_lastActiveTick = Ticks(); g_state = ACTIVE;
    GCall(Main() + OFF_START, ctrl, ToGuest(&req, sizeof req));
    if (g_cfg.kickvoice) SayWhen(V_PRESS, g_bodySpeed, "press");   // (1.1.0 user: the button's list whatever his state)
}
