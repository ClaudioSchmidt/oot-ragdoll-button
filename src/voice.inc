// oot-ragdoll, part of main.cpp: Link's voices: queue (any thread) and playback (AI thread)

struct SpinLock {   // a few instructions under it: spinning is fine
    volatile u8* f;
    explicit SpinLock(volatile u8* flag) : f(flag) { while (__atomic_test_and_set(const_cast<u8*>(f), __ATOMIC_ACQUIRE)) {} }
    ~SpinLock() { __atomic_clear(const_cast<u8*>(f), __ATOMIC_RELEASE); }
};

void Say(const char* list) {
    SpinLock k(&g_sayLock);
    u32 h = g_sayHead;
    if (h - g_sayTail >= 4) return;  // full: drop
    g_sayQ[h % 4] = {list};
    MemBarrier();
    g_sayHead = h + 1; g_lastSayTick = Ticks();
}
bool SayBusy() { return g_sayHead != g_sayTail; }
// one name, copied (the menu's play buttons: their text may change before the AI thread plays it)
[[maybe_unused]] void SayOne(const char* name) {
    static char keep[4][48]; static u32 at;
    char* k = keep[at++ % 4];
    snprintf(k, sizeof keep[0], "%s", name);
    Say(k);
}   // a voice still waits for the AI thread

// force change: entry <level> of the forcevoice list (copied, so a later file reload cannot move it)
void SayForce(int level) {
    static char item[5][48];
    const char* p = g_cfg.forcevoice;
    for (int i = 0; i < level && *p; i++) { while (*p && *p != ',') p++; if (*p == ',') p++; }
    int len = 0;
    while (p[len] && p[len] != ',' && len < 47) len++;
    if (!len) return;
    memcpy(item[level], p, len); item[level][len] = 0;
    Say(item[level]);
}

// a voice by situation (1.1.0): the list of that situation (press and kicks: of the force level now), the next entry in turn
enum VoiceWhen { V_PRESS, V_FALL, V_HARD, V_LIGHT, V_SLIDE, V_GETUP };
void SayWhen(VoiceWhen w, float speed, const char* why) {
    int f = g_cfg.force < 0 || g_cfg.force > 4 ? 2 : g_cfg.force;
    const char* lists[6] = {g_cfg.voicepress[f], g_cfg.voicefall[f], g_cfg.voicehard, g_cfg.voicelight, g_cfg.voiceslide, g_cfg.voicegetup};
    static const char* const NAMES[6] = {"button", "falling", "hard", "light", "sliding", "get-up"};
    const char* l = lists[w];
    bool off = !l[0] || !strcmp(l, "none");
    if (!off) Say(l);   // (a silent list takes no place in the queue)
    RD_LOG("[oot_ragdoll] %s: %.1f m/s -> voice %s%s%s%s", why, speed, NAMES[w], w <= V_FALL ? ", force " : "", w <= V_FALL ? FORCES[f] : "", off ? " (silent)" : "");
}

// play an SLink key on his own sound user (as the AINB node OneShotXLinkSearchAndEmit does)
bool PlayKey(uintptr_t actor, const char* key) {
    if (!actor || !key || !key[0]) return false;
    uintptr_t comp = Rd<uintptr_t>(actor + 0x270);
    comp = comp ? Rd<uintptr_t>(comp + 0x68) : 0;
    uintptr_t user = comp ? Rd<uintptr_t>(comp + 0x78) : 0;
    uintptr_t res = user ? Rd<uintptr_t>(user + 0x58) : 0;
    uintptr_t sys = res ? Rd<uintptr_t>(res + 0x18) : 0;
    if (!sys) return false;
    u64 loc = 0;
    uintptr_t gloc = ToGuest(&loc, sizeof loc);
    if (!(GCall(Main() + OFF_XL_LOOKUP, sys, gloc, ToGuest(key, strlen(key) + 1)) & 1)) return false;
    FromGuest(&loc, gloc, sizeof loc);   // the locator the lookup found
    struct { u8 a, b; u16 c; u32 d; } handle{0xff, 0, 0xffff, 0};
    GCall(Main() + OFF_XL_EMIT, user, ToGuest(&loc, sizeof loc), ToGuest(&handle, sizeof handle), u64(0));
    return true;
}

// pick one entry of a comma list (random) and play it
void PlayFromList(uintptr_t actor, const char* list) {
    if (!list || !list[0] || !strcmp(list, "none")) return;
    char items[8][48]; int n = 0;
    for (const char* p = list; *p && n < 8;) {
        int len = 0;
        while (p[len] && p[len] != ',') len++;
        if (len > 0 && len < 48) { memcpy(items[n], p, len); items[n][len] = 0; n++; }
        p += len; if (*p == ',') p++;
    }
    if (!n) return;
    // 1.1.0 (user): the entries in turn, not at random (the same voice twice in a row sounded off); a turn counter per
    // list (the lists are fixed buffers in g_cfg)
    static struct { const char* list; u32 next; } turn[48];
    u32 slot = 0;
    while (slot < 48 && turn[slot].list && turn[slot].list != list) slot++;
    if (slot == 48) slot = static_cast<u32>(reinterpret_cast<uintptr_t>(list) >> 4) % 48;   // (more lists than slots: share)
    if (turn[slot].list != list) turn[slot] = {list, 0};
    const char* k = items[turn[slot].next++ % static_cast<u32>(n)];
    if (!strncmp(k, "Voice", 5)) {  // voice AS on his voice slot, as the AS 'Voice' event does
        uintptr_t comp = actor ? Rd<uintptr_t>(actor + 0x270) : 0;
        uintptr_t pl = comp ? Rd<uintptr_t>(comp + 0x358) : 0;
        if (pl && Rd<uintptr_t>(pl + 0x18) == actor) {
            struct { uintptr_t s; u64 pad; } ref{ToGuest(k, strlen(k) + 1), 0};   // const char** asName
            g_ownVoice = true;   // (the voice hook lets our own call pass)
            GCall(Main() + OFF_VOICE_AS, pl, ToGuest(&ref, sizeof ref));
            g_ownVoice = false;
        }
        return;
    }
    if (!PlayKey(actor, k)) RD_LOG("[oot_ragdoll] voice %s not found", k);
}

// AI thread, every frame: play the next queued voice, 0.3 s apart
void VoiceTick() {
    static u64 next;
    if (g_sayTail == g_sayHead || Ticks() < next) return;
    MemBarrier();
    SayItem it = g_sayQ[g_sayTail % 4]; g_sayTail = g_sayTail + 1;
    PlayFromList(g_player, it.list);
    g_lastPlayTick = Ticks();
    next = g_lastPlayTick + TickHz() * 3 / 10;
}

// voice hook (the game's voice function main+0xa29f90, entry): ref = guest const char** asName. During a get-up after
// our ragdoll the game's VoiceDamageStand is replaced by the Getting up list (as it is: the game's own passes).
// true = skip the game's call
bool OnVoiceAs(uintptr_t pl, uintptr_t ref) {
    (void)pl;
    if (g_ownVoice || !ref || !g_getupTick || Ticks() - g_getupTick > TickHz() * 4) return false;
    uintptr_t name = Rd<uintptr_t>(ref);
    if (!name) return false;
    static const char WANT[] = "VoiceDamageStand";
    for (size_t i = 0; i < sizeof WANT; i++) if (Rd<char>(name + i) != WANT[i]) return false;
    g_getupTick = 0;   // once per get-up
    if (!strcmp(g_cfg.voicegetup, "VoiceDamageStand")) return false;   // the game's own
    SayWhen(V_GETUP, g_bodySpeed, "get-up");
    return true;
}
