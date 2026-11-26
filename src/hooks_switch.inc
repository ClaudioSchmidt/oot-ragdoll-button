// Switch: exlaunch hooks around main.cpp's On* functions; included at the end of main.cpp (under #ifndef HOOKSHOT).
// Each hook reads the same registers as the plugin always did.
namespace {

// inside the npad read loop (after one of the 5 GetNpadStates stub calls): slot x22, frame x29, stub tag [x29-0x1c]
HOOK_DEFINE_INLINE(InputHook) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        OnInput(ctx->X[22], ctx->X[29], Rd<u32>(ctx->X[29] - 0x1c));
    }
};

HOOK_DEFINE_TRAMPOLINE(ChooseHook) {
    static u64 Callback(uintptr_t node, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6, u64 a7) {
        OnChoose(node);
        return Orig(node, a1, a2, a3, a4, a5, a6, a7);
    }
};

HOOK_DEFINE_TRAMPOLINE(SlipNextHook) {
    static u64 Callback(uintptr_t self, u64 a1, u64 a2, u64 a3) {
        if (OnSlipNext()) return 0;
        return Orig(self, a1, a2, a3);
    }
};

HOOK_DEFINE_INLINE(ConsumerHook) {
    static void Callback(exl::hook::InlineCtx* ctx) { OnConsumer(ctx->X[19]); }
};

HOOK_DEFINE_INLINE(PhysAfterHook) {
    static void Callback(exl::hook::InlineCtx* ctx) { OnPhysAfter(ctx->X[19]); }
};

HOOK_DEFINE_TRAMPOLINE(CamApplyHook) {
    static void Callback(uintptr_t mgr, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6, u64 a7, float f0, float f1, float f2, float f3) {
        bool restore = OnCamApplyPre(mgr);
        Orig(mgr, a1, a2, a3, a4, a5, a6, a7, f0, f1, f2, f3);
        if (restore) OnCamApplyPost(mgr);
    }
};

HOOK_DEFINE_TRAMPOLINE(VoiceHook) {
    static u64 Callback(uintptr_t pl, uintptr_t ref, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6, u64 a7) {
        if (OnVoiceAs(pl, ref)) return 0;
        return Orig(pl, ref, a2, a3, a4, a5, a6, a7);
    }
};

HOOK_DEFINE_INLINE(RagUpdateHook) {
    static void Callback(exl::hook::InlineCtx* ctx) { OnRagUpdate(ctx->X[0]); }
};

}  // namespace

// HSML 2.1: this module carries no menu. Its page goes into HSML's Mod Menu (hsml switch/client/hsml_client.inc,
// build.sh copies it), hosted by the HSML module or by a released mod that carries the menu; it is registered as on PC
// (HsmlMenuRegister, exl_main). Without a host: one log line, no menu, the settings file works as before.
#define HSML_NX_LOG(...) Logging.Log(__VA_ARGS__)   // (the client's own lines: the emulator log; it forwards them itself)
#define HSML_NX_LOG_FORWARD (g_tune.logRag == 0 && g_tune.logMap == 0)   // RD_LOG's lines on HSML's Log page only without the debug logs
#define HSML_NX_TAG "[oot_ragdoll]"
#define HSML_NX_SD "ragdollsd"
#define HSML_NX_SD_OK CfgSdOk()
#define HSML_NX_NAME "Ragdoll Button"
#define HSML_NX_VERSION MOD_VERSION
#define HSML_NX_AUTHOR "ClaudioSchmidt"
#define HSML_NX_META "link = GameBanana | https://gamebanana.com/mods/740650\nlink = GitHub | https://github.com/ClaudioSchmidt/oot-ragdoll-button\nlicense = GPL-2.0\n"   // more info rows (hsml switch/hsml_nx.h)
#define HSML_NX_ABOUT "Press a button and Link goes limp: a full physics ragdoll from any pose, keeping his speed."
#include "hsml_client.inc"
namespace {
void NxRdPageDraw(void*) { RdMenuPage(); }
int NxRdPageDirty(void*) { return RdMenuDirty(); }
void NxRdPageSave(void*) { RdMenuSave(); }
// the pads' buttons on every change, from the hosting module. OnInput reads the buttons itself here and gives a
// listening button row its button; the menu must not act on that press (nor on its release a moment later)
int NxRdPageInput(void*, int kind, int, int) {
    if (kind != HSML_IN_PAD) return 0;
    if (!MenuOpen()) { RdMenuClosed(); return 0; }
    return g_bindTarget != nullptr || Ticks() - g_bindDoneTick < TickHz() / 2;
}
}  // namespace

extern "C" void exl_main(void* x0, void* x1) {
    exl::hook::Initialize();
    // the Mod Menu: the module that hosts it (HSML's), then this mod's page. Before the version check: a mod that stays
    // off still shows in the list
    const HsmlApi* api = HsmlNxInstall();
    static const HsmlMenuExtra extra = {nullptr, nullptr, NxRdPageInput};
    bool menu = HsmlMenuRegister(api, "ragdoll-button", NxRdPageDraw, NxRdPageDirty, NxRdPageSave, nullptr, &extra);
    RD_LOG("[oot_ragdoll] settings page %s%s%s", menu ? "registered (the Mod Menu is hosted by " : "not registered (no Mod Menu)",
           menu ? HsmlNxHostName() : "", menu ? ")" : "");
    if (!RagCheck()) return;
    InputHook::InstallAtOffset(OFF_INPUT);
    ChooseHook::InstallAtOffset(OFF_CHOOSE);
    SlipNextHook::InstallAtOffset(OFF_SLIP_NEXT);
    ConsumerHook::InstallAtOffset(OFF_CONSUMER);
    PhysAfterHook::InstallAtOffset(OFF_PHYS_AFTER);
    RagUpdateHook::InstallAtOffset(OFF_RAGUPDATE);
    CamApplyHook::InstallAtOffset(OFF_CAM_APPLY);
    VoiceHook::InstallAtOffset(OFF_VOICE_AS);
    RD_LOG("[oot_ragdoll] %s: hooks installed, tune @ %p", MOD_VERSION, (void*)&g_tune);
}

extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("Default exception handler called!");
}
