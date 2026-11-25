// oot-ragdoll, part of main.cpp: enemy ragdolls, TEST BUILD (2026-11-22, Hookshot only; FINDINGS.md "Enemy ragdolls").
// Keys (game window, menu open or not): F6 = the nearest enemy goes limp (reaction list FullDynamic), F8 = every enemy
// within EN_ALL_M (FullDynamic); the menu's Debug tab has the same as buttons, plus SmallDamage (F7 is Triforce's).
// 2026-11-24: the controller update below never ran in the Lizalfos fight (Triforce), so the enemies are now also found
// from the generic actor update main+0x2c1d740(actor) (every actor, every frame; it calls the controller update itself
// only on one of its paths): comp = [[actor+0x270]+0x390], as the game reads it there. The start is the game's own call with the request its
// auto reaction builds (main+0x2e8f690); afterwards every state change of that enemy's controller is logged
// (ragdoll.log `enemy ...`) until it is back at 0 or 60 s passed. Question for the test: does the enemy get up / fight
// again by itself, after how long, and with a snap or an animation.
#ifdef HOOKSHOT
// RagdollReactionController: one per actor with a ragdoll reaction (enemies, a few NPCs), at [[actor+0x270]+0x390].
// Its update main+0xf98ff0(comp, ctx) runs every frame for every such actor (from the actor update, main+0x2c1e3c0):
// comp+0x18 = actor, +0x20 = state (0 none, 3 = ragdoll; 1 / 3 run the ragdoll update main+0x2e8f1f0, 2 its own path)
constexpr uintptr_t OFF_EN_UPDATE = 0xf98ff0;
constexpr uintptr_t OFF_ACTOR_UPDATE = 0x2c1d740;   // actor update (actor, ctx); [actor+0x270] = components, +0x390 the controller
constexpr uintptr_t OFF_EN_START  = 0x2e916b0;   // start (comp, const EnReq*)
constexpr uintptr_t OFF_EN_VT     = 0x4b4ed30;   // the component's vtable
constexpr uintptr_t OFF_STR_FULLDYN = 0x3d3f3af, OFF_STR_SMALLDMG = 0x3d7f911, OFF_STR_EMPTY = 0x3ee4f8c;  // in main's rodata
constexpr float EN_NEAR_M = 30.f, EN_ALL_M = 20.f;

// the request (0x38 bytes), as main+0x2e8f690 builds it
struct EnReq {
    u64 list;           // +0x00 reaction list name ("FullDynamic")
    u64 s08, s10;       // +0x08 / +0x10 "" (+0x10: forced AnimationDrive key)
    float velScale;     // +0x18
    float rot[3];       // +0x1c direction of the operation
    u8 b28, b29, b2a, b2b;   // +0x2a = 1
    u32 i2c[2];         // +0x2c = -1 (8 bytes, unaligned in the game's layout)
    u32 u34;            // +0x34 = 0xffff
};
static_assert(sizeof(EnReq) == 0x38, "EnReq");

struct EnSeen { uintptr_t comp, actor; u64 tick; };
EnSeen g_en[64];                 // controllers updated lately (any thread; a stale entry costs nothing)
volatile u32 g_enReq;            // 0 none, 1 nearest FullDynamic, 2 nearest SmallDamage, 3 all near FullDynamic
volatile u64 g_enReqTick;
struct EnWatch { uintptr_t comp, actor; u64 start, last; u32 state; float pos[3]; };
EnWatch g_enWatch[8];
volatile u8 g_enLock;
volatile bool g_enSeenAny;
char g_enLast[160];             // the menu's status line: the last start or miss
volatile u32 g_enStarts;
// 1.0.4: the 3 nearest actors other than Link while a request waits (diagnostic dump; 1.0.3 dumped Link himself)
uintptr_t g_enNearA[3]; float g_enNearD[3] = {1e9f, 1e9f, 1e9f};
inline float EnDist(uintptr_t actor);
void EnNearNote(uintptr_t actor) {
    if (actor == g_player) return;
    float d = EnDist(actor);
    if (d < 0.3f || d >= g_enNearD[2]) return;
    for (int i = 0; i < 3; i++) if (g_enNearA[i] == actor) return;
    int k = 2;
    for (; k > 0 && g_enNearD[k - 1] > d; k--) { g_enNearD[k] = g_enNearD[k - 1]; g_enNearA[k] = g_enNearA[k - 1]; }
    g_enNearD[k] = d; g_enNearA[k] = actor;
}
volatile u32 g_actorCalls;      // actor updates seen while busy (diagnostic)

inline float EnDist(uintptr_t actor) {
    uintptr_t p = g_player;
    if (!p || !actor) return 1e9f;
    float d = 0;
    for (int i = 0; i < 3; i++) { float v = Rd<float>(actor + 0x31c + 4 * i) - Rd<float>(p + 0x31c + 4 * i); d += v * v; }
    return sqrtf(d);
}
void EnNote(uintptr_t comp, uintptr_t actor, u64 now) {
    int free = -1;
    for (int i = 0; i < 64; i++) {
        if (g_en[i].comp == comp) { g_en[i].actor = actor; g_en[i].tick = now; return; }
        if (free < 0 && now - g_en[i].tick > TickHz()) free = i;
    }
    if (free >= 0) g_en[free] = {comp, actor, now};
}
void EnStart(uintptr_t comp, uintptr_t actor, u32 kind, u64 now) {
    EnReq r{};
    r.list = Main() + (kind == 2 ? OFF_STR_SMALLDMG : OFF_STR_FULLDYN);
    r.s08 = r.s10 = Main() + OFF_STR_EMPTY;
    r.b2a = 1; r.i2c[0] = r.i2c[1] = 0xffffffffu; r.u34 = 0xffff;
    u32 before = Rd<u32>(comp + 0x20);
    GCall(Main() + OFF_EN_START, comp, ToGuest(&r, sizeof r));
    u32 after = Rd<u32>(comp + 0x20);
    RD_LOG("[oot_ragdoll] enemy %lx (actor %lx, %.1f m): start %s, state %u -> %u", comp, actor, EnDist(actor),
           kind == 2 ? "SmallDamage" : "FullDynamic", before, after);
    snprintf(g_enLast, sizeof g_enLast, "started %s at %.1f m (state %u -> %u)", kind == 2 ? "SmallDamage" : "FullDynamic", EnDist(actor), before, after);
    Bump(&g_enStarts);
    while (__atomic_test_and_set(&g_enLock, __ATOMIC_ACQUIRE)) {}
    int slot = 0;
    for (int i = 0; i < 8; i++) {
        if (g_enWatch[i].comp == comp) { slot = i; break; }
        if (g_enWatch[i].start < g_enWatch[slot].start) slot = i;
    }
    EnWatch& w = g_enWatch[slot];
    w.comp = comp; w.actor = actor; w.start = now ? now : 1; w.last = now; w.state = after;
    for (int i = 0; i < 3; i++) w.pos[i] = Rd<float>(actor + 0x31c + 4 * i);
    __atomic_clear(&g_enLock, __ATOMIC_RELEASE);
}
// a started enemy: its state changes, how far it moved, until it is back at 0 (or 60 s)
void EnWatchTick(uintptr_t comp, uintptr_t actor, u64 now) {
    for (auto& w : g_enWatch) {
        if (w.comp != comp || !w.start) continue;
        float t = static_cast<float>(now - w.start) / TickHz();
        u32 st = Rd<u32>(comp + 0x20);
        float mv = 0;
        for (int i = 0; i < 3; i++) { float v = Rd<float>(actor + 0x31c + 4 * i) - w.pos[i]; mv += v * v; }
        mv = sqrtf(mv);
        if (st != w.state) {
            RD_LOG("[oot_ragdoll] enemy %lx: state %u -> %u after %.2f s (moved %.1f m)", comp, w.state, st, t, mv);
            w.state = st;
        } else if (now - w.last > TickHz()) {
            RD_LOG("[oot_ragdoll] enemy %lx: state %u at %.1f s (moved %.1f m)", comp, st, t, mv);
        } else continue;
        w.last = now;
        if (st == 0 || t > 60) { RD_LOG("[oot_ragdoll] enemy %lx: watch ends (%s)", comp, st == 0 ? "state 0" : "60 s"); w.start = 0; }
    }
}
// the controller update, before the original (the game itself starts ragdolls from inside it)
void OnEnemyUpdate(uintptr_t comp) {
    if (!comp || Rd<uintptr_t>(comp) != Main() + OFF_EN_VT) return;
    uintptr_t actor = Rd<uintptr_t>(comp + 0x18);
    if (!actor) return;
    u64 now = Ticks();
    if (!g_enSeenAny) { g_enSeenAny = true; RD_LOG("[oot_ragdoll] enemy hook: first controller update (comp %lx)", comp); }
    EnNote(comp, actor, now);
    EnWatchTick(comp, actor, now);
    u32 req = g_enReq;
    if (!req) return;
    u64 age = now - g_enReqTick;
    if (age < TickHz() / 10) return;   // 0.1 s: every enemy near has been seen since the press
    if (req == 3) {
        if (age > TickHz() * 3 / 10) { __atomic_compare_exchange_n(const_cast<u32*>(&g_enReq), &req, 0u, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return; }
        if (EnDist(actor) > EN_ALL_M) return;
        for (auto& w : g_enWatch) if (w.comp == comp && w.start >= g_enReqTick) return;   // already started for this press
        EnStart(comp, actor, 1, now);
        return;
    }
    // nearest of those updated in the last 0.3 s
    uintptr_t best = 0; float bd = EN_NEAR_M;
    for (auto& e : g_en) {
        if (!e.comp || now - e.tick > TickHz() * 3 / 10) continue;
        float d = EnDist(e.actor);
        if (d < bd) { bd = d; best = e.comp; }
    }
    if (!best) {
        if (__atomic_compare_exchange_n(const_cast<u32*>(&g_enReq), &req, 0u, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        {
            RD_LOG("[oot_ragdoll] enemy: no enemy with a ragdoll within %.0f m (Link %s; actor updates seen %u)", EN_NEAR_M, g_player ? "known" : "not known yet", static_cast<unsigned>(g_actorCalls));
            snprintf(g_enLast, sizeof g_enLast, "no enemy with a ragdoll within %.0f m", EN_NEAR_M);
        }
        return;
    }
    if (best != comp) return;
    if (!__atomic_compare_exchange_n(const_cast<u32*>(&g_enReq), &req, 0u, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) return;
    EnStart(comp, actor, req, now);
}
// the actor update: its enemy controller, if it has one. Only while a request waits or a started enemy is watched
void EnDumpNearest();
bool EnemyBusy() {
    if (u32 req = g_enReq) {
        // no actor with a controller answered within 2 s: give up (else the request would wait forever)
        if (Ticks() - g_enReqTick > 2 * TickHz() &&
            __atomic_compare_exchange_n(const_cast<u32*>(&g_enReq), &req, 0u, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            RD_LOG("[oot_ragdoll] enemy: no enemy controller answered in 2 s (actor updates seen %u, controller seen %s)",
                   static_cast<unsigned>(g_actorCalls), g_enSeenAny ? "yes" : "never");
            snprintf(g_enLast, sizeof g_enLast, "no enemy found (%u actor updates, controller %s); components of the nearest actor are in ragdoll.log", static_cast<unsigned>(g_actorCalls), g_enSeenAny ? "seen" : "never seen");
            EnDumpNearest();
            for (int i = 0; i < 3; i++) { g_enNearA[i] = 0; g_enNearD[i] = 1e9f; }
            return false;
        }
        return true;
    }
    for (auto& w : g_enWatch) if (w.start) return true;
    return false;
}
// 1.0.3 (user's run: 45000 actor updates, no controller at [[actor+0x270]+0x390]): the controller's slot is searched in
// the component holder (first 0x1000 bytes) for a pointer whose vtable is the controller's; the slot found is kept
// (g_enOff). A pointer is only read when it lies within 4 GB of the actor (heap), 8-aligned
volatile u32 g_enOff = 0x390;
volatile bool g_enOffFound;
inline bool EnPlausible(uintptr_t p, uintptr_t actor) {
    return p && !(p & 7) && (p > actor ? p - actor : actor - p) < 0x100000000ull && HsReadable(p) && HsReadable(p + 0x20);
}
void OnActorUpdate(uintptr_t actor) {
    g_actorCalls = g_actorCalls + 1;
    uintptr_t h = actor ? Rd<uintptr_t>(actor + 0x270) : 0;
    if (!EnPlausible(h, actor)) return;
    const uintptr_t vt = Main() + OFF_EN_VT;
    uintptr_t c = Rd<uintptr_t>(h + g_enOff);
    if (EnPlausible(c, actor) && Rd<uintptr_t>(c) == vt) { OnEnemyUpdate(c); return; }
    if (g_enReq) EnNearNote(actor);
    if (g_enOffFound) return;
    for (u32 off = 0; off < 0x1000; off += 8) {
        uintptr_t q = Rd<uintptr_t>(h + off);
        if (!EnPlausible(q, actor) || Rd<uintptr_t>(q) != vt) continue;
        g_enOff = off; g_enOffFound = true;
        RD_LOG("[oot_ragdoll] enemy: the controller sits at holder+0x%x (comp %lx, its actor %s)", off, q, Rd<uintptr_t>(q + 0x18) == actor ? "matches" : "DIFFERS");
        OnEnemyUpdate(q);
        return;
    }
}
// after a miss: the nearest actor's components (holder slots pointing at an object whose +0x18 is the actor), with
// their vtables as main offsets, so the controller (or what stands for it) can be named
void EnDumpOne(uintptr_t actor, float dist) {
    uintptr_t h = Rd<uintptr_t>(actor + 0x270);
    RD_LOG("[oot_ragdoll] enemy dump: actor %lx at %.1f m, holder %lx", actor, dist, h);
    if (!EnPlausible(h, actor)) return;
    int n = 0;
    for (u32 off = 0; off < 0x1000 && n < 60; off += 8) {
        uintptr_t q = Rd<uintptr_t>(h + off);
        if (!EnPlausible(q, actor)) continue;
        uintptr_t v = Rd<uintptr_t>(q);
        bool own = Rd<uintptr_t>(q + 0x18) == actor;
        if (!own) continue;
        RD_LOG("[oot_ragdoll] enemy dump: holder+0x%x -> %lx vtable main+0x%lx", off, q, v - Main());
        n++;
    }
    RD_LOG("[oot_ragdoll] enemy dump: %d components (expected controller vtable main+0x%lx)", n, OFF_EN_VT);
}
void EnDumpNearest() {
    if (!g_enNearA[0]) { RD_LOG("[oot_ragdoll] enemy dump: no actor other than Link updated near him"); return; }
    for (int i = 0; i < 3; i++) if (g_enNearA[i]) EnDumpOne(g_enNearA[i], g_enNearD[i]);
}
// a start request (keys: window thread; menu: render thread): 1 nearest FullDynamic, 2 nearest SmallDamage, 3 all near
void EnemyRequest(u32 kind, const char* from) {
    if (!g_enSeenAny) RD_LOG("[oot_ragdoll] enemy %s: no enemy controller seen yet (looked for from now on)", from);
    u64 t = Ticks();
    g_enReqTick = t ? t : 1;
    g_enReq = kind;
    snprintf(g_enLast, sizeof g_enLast, "looking for an enemy...");
    RD_LOG("[oot_ragdoll] enemy %s: %s", from, kind == 1 ? "nearest, FullDynamic" : kind == 2 ? "nearest, SmallDamage" : "all near, FullDynamic");
}
// window thread: F6 / F8. True = the key was ours
bool EnemyKey(int vk) {
    if (vk == 0x75) EnemyRequest(1, "key F6");
    else if (vk == 0x77) EnemyRequest(3, "key F8");
    else return false;
    return true;
}
#endif
