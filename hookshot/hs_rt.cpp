// Hookshot runtime for oot-ragdoll: a HSML mod (<Hookshot>\mods\<name>\hookshot\ragdoll-button.dll), loaded by the HSML
// loader (bin\dbghelp.dll). Hookshot compiles the game ahead of time to x64: every guest function f_<guest addr>
// is `u64 f(Ctx* rcx)`; the loader installs the detours below at function starts. The generic runtime (guest calls,
// ToGuest, log, HookGuest) is ../../hsml/sdk/hsml_mod.cpp. main.cpp's hook bodies are reached through the
// Hs* entry points of hooks_hs.inc. The settings menu is a page in HSML's shared menu (src/menu.inc, registered below). Log: <Hookshot>\ragdoll.log. Settings: <modDir>\oot_ragdoll.txt.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "hsml_mod.hpp"
#include "hsml_menu.hpp"

bool HsRagInit();
bool HsRagCheck();
void HsOnInput(u64 slot, uintptr_t fp, u32 tag);
void HsOnChoose(uintptr_t node);
bool HsOnSlipNext();
void HsOnConsumer(uintptr_t cont);
void HsOnPhysAfter(uintptr_t cont);
void HsOnRagUpdate(uintptr_t x0);
bool HsOnVoiceAs(uintptr_t pl, uintptr_t ref);
bool HsOnCamApplyPre(uintptr_t mgr);
void RdMenuPage();          // src/menu.inc: the page, Save and the unsaved mark, the menu key, a button row's listening
int RdMenuDirty();
void RdMenuSave();
int RdMenuKey();
int RdMenuKeyEvent(int kind, int vk);   // 1 = the mod's key, -1 = seen but the game gets it too, 0 = not ours
void RdMenuClosed();
bool RdMenuBindPad(u64 held);
void HsOnCamApplyPost(uintptr_t mgr);
void HsOnEnemyUpdate(uintptr_t comp);
bool HsEnemyKey(int vk);
bool HsEnemyBusy();
void HsOnActorUpdate(uintptr_t actor);

namespace {

constexpr u64 M = HS_MAIN;
const u64 F_ACTOR = M + 0x2c1d740;  // enemy test build: the actor update (actor, ctx)
const u64 F_ENEMY = M + 0xf98ff0;   // enemy test build: RagdollReactionController update (comp, ctx)
const u64 F_VOICE = M + 0xa29f90;   // the player voice function (playerObj, const char** asName): get-up voice
const u64 F_CHOOSE = M + 0x2b30de0, F_SLIP = M + 0x2b3d5f0, F_RAG = M + 0x321c040, F_CAM = M + 0x4c48c0;
// The game's import thunks (f_83c62xxx: set x16/x17, jmp) are skipped by Hookshot's compiler: the game calls the
// forwarder behind each one directly (f_8566xxxx = jmp to Hookshot's nn::hid; f_8572f300 = memset). So the hooks sit
// on those; HSML moves the forwarder's jmp into the trampoline.
const u64 F_CONSUMER = 0x838f5b70, F_PHYS = 0x80175c60, F_MEMSET = 0x8572f300;   // thunk f_83c62360
const u64 F_NPAD[5] = {0x8566b410, 0x8566ac30, 0x8566b500, 0x8566b5f0, 0x8566ad20};   // thunks f_83c627f0, 800, 810, 820, 830
const u64 LR_CONSUMER = 0x801784c0;   // return address of the op-apply call at main+0x1584bc
const u64 LR_MEMSET = 0x80176584;     // return address of the memset call at main+0x156580
// return addresses in the npad loop after the 5 stub calls -> style tag ([x29-0x1c], index into OFF_NPAD_STUBS)
constexpr struct { u64 lr; u32 tag; } NPAD_LR[5] = {{0x834894fc, 2}, {0x83489528, 3}, {0x83489554, 4}, {0x83489670, 0}, {0x8348969c, 1}};

volatile bool g_ok;   // all hooks installed; until then (or if one failed) every detour only passes through
// Game version: checked at the first hook call, when guest memory holds the game (at HsmlInit it does not yet).
// 0 not checked yet, 1 running, -1 off (another game build: the detours only pass through)
volatile LONG g_checked;
bool Ready() {
    if (!g_ok) return false;
    if (g_checked == 0) {
        static volatile LONG once;
        if (InterlockedExchange(&once, 1)) return false;   // another thread is checking right now
        bool ok = HsRagCheck();
        HsLog("[oot_ragdoll] game version check: %s", ok ? "ok, running" : "FAILED, the mod stays off");
        g_checked = ok ? 1 : -1;
    }
    return g_checked == 1;
}
char g_modDir[MAX_PATH];

GuestFn o_enemy, o_actor, o_voice;
GuestFn o_choose, o_slip, o_rag, o_cam, o_consumer, o_phys, o_memset, o_npad[5];

u64 D_choose(Ctx* c, u64 a, u64 b, u64 d) {
    if (Ready()) { InHook h(c); HsOnChoose(c->x[0]); }
    return o_choose(c, a, b, d);
}
u64 D_enemy(Ctx* c, u64 a, u64 b, u64 d) {
    if (Ready()) { InHook h(c); HsOnEnemyUpdate(c->x[0]); }
    return o_enemy(c, a, b, d);
}
u64 D_actor(Ctx* c, u64 a, u64 b, u64 d) {
    if (g_checked == 1 && HsEnemyBusy()) { InHook h(c); HsOnActorUpdate(c->x[0]); }
    return o_actor(c, a, b, d);
}
u64 D_slip(Ctx* c, u64 a, u64 b, u64 d) {
    if (Ready()) {
        bool stay;
        { InHook h(c); stay = HsOnSlipNext(); }
        if (stay) { c->x[0] = 0; return 0; }   // nothing new: stay down, the original is not called
    }
    return o_slip(c, a, b, d);
}
u64 D_voice(Ctx* c, u64 a, u64 b, u64 d) {
    if (Ready()) {
        bool skip;
        { InHook h(c); skip = HsOnVoiceAs(c->x[0], c->x[1]); }
        if (skip) { c->x[0] = 0; return 0; }   // the Getting up list plays instead
    }
    return o_voice(c, a, b, d);
}
u64 D_rag(Ctx* c, u64 a, u64 b, u64 d) {
    if (Ready()) { InHook h(c); HsOnRagUpdate(c->x[0]); }
    return o_rag(c, a, b, d);
}
u64 D_cam(Ctx* c, u64 a, u64 b, u64 d) {
    u64 mgr = c->x[0];
    bool restore = false;
    if (Ready()) { InHook h(c); restore = HsOnCamApplyPre(mgr); }
    u64 r = o_cam(c, a, b, d);
    if (restore) { InHook h(c); HsOnCamApplyPost(mgr); }
    return r;
}

// The 5 GetNpadStates stubs: called from the npad loop (lr = return address there) = the Switch's InputHook site.
// OnInput itself calls the same stubs again for the raw buttons (through CallGuest, the Ctx still holding the loop's
// lr): inInput skips that nested round. lr, x22 and fp are read before the original (x22 / fp are callee-saved).
thread_local bool t_inInput;
template<int K> u64 D_npad(Ctx* c, u64 a, u64 b, u64 d) {
    u64 lr = c->lr, slot = c->x[22], fp = c->fp;
    u64 r = o_npad[K](c, a, b, d);
    if (!t_inInput)
        for (auto& e : NPAD_LR) if (e.lr == lr) {
            if (!Ready()) break;
            t_inInput = true;
            { InHook h(c); HsOnInput(slot, fp, e.tag); }
            t_inInput = false;
            break;
        }
    return r;
}

// Op-apply call of the physics push block (once per op): after it, the kick is applied as the Switch's hook after the
// loop did. NOTE: with an empty op list this is never called (the Switch hook still ran): HsOnPhysAfter falls back.
u64 D_consumer(Ctx* c, u64 a, u64 b, u64 d) {
    u64 lr = c->lr, cont = c->x[19];
    u64 r = o_consumer(c, a, b, d);
    if (lr == LR_CONSUMER && Ready()) { InHook h(c); HsOnConsumer(cont); }
    return r;
}

// Physics update f_80175c60(container): its frame is {container, pending}. The memset at main+0x156580 stands where the
// Switch's PhysAfter hook ran (x19 = container): OnPhysAfter runs there, before the memset. When that call is skipped
// (two branches), it runs after the function returns instead.
struct PhysFrame { u64 cont; bool pending; };
thread_local PhysFrame t_pf[8];
thread_local int t_pfn;
u64 D_phys(Ctx* c, u64 a, u64 b, u64 d) {
    if (g_checked != 1 || t_pfn >= 8) return o_phys(c, a, b, d);   // (the check itself runs in the AI / input detours)
    u64 cont = c->x[0];
    int me = t_pfn++;
    t_pf[me] = {cont, true};
    u64 r = o_phys(c, a, b, d);
    t_pfn = me;
    if (t_pf[me].pending) { InHook h(c); HsOnPhysAfter(cont); }
    return r;
}
u64 D_memset(Ctx* c, u64 a, u64 b, u64 d) {   // memset: compare lr first, nothing else on the common path
    if (c->lr == LR_MEMSET && t_pfn) {
        PhysFrame& f = t_pf[t_pfn - 1];
        if (f.pending && f.cont == c->x[19]) {
            f.pending = false;
            u64 cont = f.cont;
            { InHook h(c); HsOnPhysAfter(cont); }
        }
    }
    return o_memset(c, a, b, d);
}

// ---- the page in HSML's menu ----
void PageDraw(void*) { RdMenuPage(); }
int PageDirty(void*) { return RdMenuDirty(); }
void PageSave(void*) { RdMenuSave(); }
int PageInput(void*, int kind, int a, int b) {
    if (kind == HSML_IN_KEY_DOWN || kind == HSML_IN_KEY_UP || kind == HSML_IN_FOCUS_LOST) {   // a key cell listening, the action keys
        int r = RdMenuKeyEvent(kind, a);
        if (r) return r > 0 ? 1 : 0;
    }
    if (kind == HSML_IN_KEY_DOWN && a && a == RdMenuKey()) { HsmlMenuOpen(); return 1; }
    if (kind == HSML_IN_KEY_DOWN && g_checked == 1 && (o_enemy || o_actor) && HsEnemyKey(a)) return 1;   // enemy test build: F6 / F8
    if (kind == HSML_IN_PAD) {
        if (!HsmlMenuIsOpen()) { RdMenuClosed(); return 0; }
        u64 held = static_cast<u32>(a) | (static_cast<u64>(static_cast<u32>(b)) << 32);
        if (HsmlMenuTakesPad()) return RdMenuBindPad(held) ? 1 : 0;   // (else OnInput has the buttons itself)
    }
    return 0;
}

bool Install() {
    const u64 need[] = {F_CHOOSE, F_SLIP, F_RAG, F_CAM, F_CONSUMER, F_PHYS, F_MEMSET, F_NPAD[0], F_NPAD[1], F_NPAD[2], F_NPAD[3], F_NPAD[4]};
    bool ok = true;
    for (u64 g : need) if (!HostOfGuest(g)) { HsLog("f_%lx missing (Hookshot build changed)", g); ok = false; }
    if (!ok) return false;
    o_choose = HookGuest("choose", F_CHOOSE, reinterpret_cast<void*>(D_choose));
    o_slip = HookGuest("slipnext", F_SLIP, reinterpret_cast<void*>(D_slip));
    o_rag = HookGuest("ragupdate", F_RAG, reinterpret_cast<void*>(D_rag));
    o_cam = HookGuest("camapply", F_CAM, reinterpret_cast<void*>(D_cam));
    o_consumer = HookGuest("consumer", F_CONSUMER, reinterpret_cast<void*>(D_consumer));
    o_phys = HookGuest("physafter", F_PHYS, reinterpret_cast<void*>(D_phys));
    o_memset = HookGuest("physafter-memset", F_MEMSET, reinterpret_cast<void*>(D_memset));
    void* npad[5] = {reinterpret_cast<void*>(D_npad<0>), reinterpret_cast<void*>(D_npad<1>), reinterpret_cast<void*>(D_npad<2>),
                     reinterpret_cast<void*>(D_npad<3>), reinterpret_cast<void*>(D_npad<4>)};
    for (int i = 0; i < 5; i++) o_npad[i] = HookGuest("npad", F_NPAD[i], npad[i]);
    ok = o_choose && o_slip && o_rag && o_cam && o_consumer && o_phys && o_memset;
    for (auto f : o_npad) ok = ok && f;
    // get-up voice: optional (without it the game's own plays)
    o_voice = HostOfGuest(F_VOICE) ? HookGuest("voice", F_VOICE, reinterpret_cast<void*>(D_voice)) : nullptr;
    HsLog("[oot_ragdoll] voice hook (get-up voice) %s", o_voice ? "installed" : "MISSING: the game's own get-up voice plays");
    // enemy test build: optional (the rest runs without it)
    o_enemy = HostOfGuest(F_ENEMY) ? HookGuest("enemyupdate", F_ENEMY, reinterpret_cast<void*>(D_enemy)) : nullptr;
    o_actor = HostOfGuest(F_ACTOR) ? HookGuest("actorupdate", F_ACTOR, reinterpret_cast<void*>(D_actor)) : nullptr;
    HsLog("[oot_ragdoll] enemy test hooks: controller update %s, actor update %s (F6 nearest, F8 all within 20 m)", o_enemy ? "installed" : "MISSING", o_actor ? "installed" : "MISSING");
    return ok;
}

}  // namespace

// enemy test build: a guest address is committed, readable memory (VirtualQuery, regions cached: the heap is a few big ones)
bool HsReadable(uintptr_t g) {
    struct Reg { u64 lo, hi; };
    static Reg cache[32]; static unsigned next;
    for (auto& r : cache) if (g >= r.lo && g < r.hi) return true;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(reinterpret_cast<void*>(g + HS_MEM), &mbi, sizeof mbi)) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) || !mbi.Protect) return false;
    u64 lo = reinterpret_cast<u64>(mbi.BaseAddress) - HS_MEM;
    cache[next++ % 32] = {lo, lo + mbi.RegionSize};
    return true;
}

// ---- settings file <modDir>\oot_ragdoll.txt ------------------------------------------------
bool CfgLoad(char* buf, size_t cap, size_t* len) {
    char path[MAX_PATH + 32]; snprintf(path, sizeof path, "%s\\oot_ragdoll.txt", g_modDir);
    FILE* f = fopen(path, "rb");
    if (!f) { HsLog("[oot_ragdoll] oot_ragdoll.txt not found (%s), defaults kept", path); return false; }
    size_t n = fread(buf, 1, cap - 1, f); fclose(f);
    buf[n] = 0; *len = n;
    return true;
}
bool CfgSave(const char* buf, size_t len) {
    char path[MAX_PATH + 32]; snprintf(path, sizeof path, "%s\\oot_ragdoll.txt", g_modDir);
    FILE* f = fopen(path, "wb");
    if (!f) { HsLog("[oot_ragdoll] could not open oot_ragdoll.txt for writing (%s)", path); return false; }
    size_t n = fwrite(buf, 1, len, f); fclose(f);
    if (n != len) { HsLog("[oot_ragdoll] could not write oot_ragdoll.txt"); return false; }
    return true;
}

extern "C" __declspec(dllexport) int HsmlInit(const HsmlApi* api, const char* modDir) {
    if (!HsmlModStart(api, "ragdoll-button", "ragdoll.log")) return 0;
    snprintf(g_modDir, sizeof g_modDir, "%s", modDir ? modDir : ".");
    HsLog("[oot_ragdoll] ragdoll-button (HSML mod) from %s", g_modDir);
    if (!HsRagInit()) return 0;   // (reads the settings file)
    static const HsmlMenuExtra extra = {nullptr, nullptr, PageInput};
    bool menu = HsmlMenuRegister(api, "ragdoll-button", PageDraw, PageDirty, PageSave, nullptr, &extra);
    HsLog("[oot_ragdoll] settings page: %s", menu ? "in the HSML menu" : "none (no HSML menu here: HSML before 2.0, or hsml-ui.dll missing / off); the settings file works");
    bool ok = Install();
    g_ok = ok;
    HsLog("[oot_ragdoll] hooks %s", ok ? "installed" : "MISSING or failed: off, the installed ones only pass through");
    return ok ? 1 : 0;
}
