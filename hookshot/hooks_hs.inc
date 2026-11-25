// Hookshot: entry points for hs_rt.cpp's detours, included at the end of main.cpp (outside its anonymous namespace,
// so they reach the On* functions). Where each Switch hook site went (M = guest 0x80020000):
//   ChooseHook    main+0x2b30de0 trampoline          -> entry of f_82b50de0, node = x0
//   SlipNextHook  main+0x2b3d5f0 trampoline          -> entry of f_82b5d5f0 (true = return 0)
//   RagUpdateHook main+0x321c040 (first instruction) -> entry of f_8323c040, ctrl = x0
//   CamApplyHook  main+0x4c48c0 trampoline           -> f_804e48c0: Pre before, Post after the original
//   InputHook     main+0x346957c (in the npad loop)  -> exit of the 5 GetNpadStates stubs f_83c627f0.. when called from
//                 the loop (lr = return address in the loop): slot = x22, frame = x29, tag = which call site
//   ConsumerHook  main+0x1584c4 (after the push-op loop) -> exit of the op-apply call f_838f5b70 at main+0x1584bc
//                 (per op, not once after the loop: OnConsumer writes the same start kick each time)
//   PhysAfterHook main+0x1564b8 (x19 = container)    -> f_80175c60 (container = x0): before the memset at
//                 main+0x156580, or after the function returns when that call is skipped
// At HsmlInit: the settings file (plain file access), so the menu and the version check have it from the start.
bool HsRagInit() {
    LoadConfig();
    RD_LOG("[oot_ragdoll] %s (Hookshot): tune @ %p", MOD_VERSION, static_cast<void*>(&g_tune));
    return true;
}
// at the first hook call (guest memory is there now): the hook sites' fingerprints; false = the mod stays off
bool HsRagCheck() { return RagCheck(); }
void HsOnInput(u64 slot, uintptr_t fp, u32 tag) { OnInput(slot, fp, tag); }
void HsOnChoose(uintptr_t node) { OnChoose(node); }
bool HsOnSlipNext() { return OnSlipNext(); }
void HsOnConsumer(uintptr_t cont) { OnConsumer(cont); }
// (with an empty op list the op-apply detour never runs: OnPhysAfter's start kick fallback covers that on both platforms)
void HsOnPhysAfter(uintptr_t cont) { OnPhysAfter(cont); }
void HsOnRagUpdate(uintptr_t x0) { OnRagUpdate(x0); }
// 1.1.0: main+0xa29f90 (the player voice function) -> entry of f_80a49f90, playerObj = x0, const char** = x1 (true = skip it)
bool HsOnVoiceAs(uintptr_t pl, uintptr_t ref) { return OnVoiceAs(pl, ref); }
bool HsOnCamApplyPre(uintptr_t mgr) { return OnCamApplyPre(mgr); }
void HsOnCamApplyPost(uintptr_t mgr) { OnCamApplyPost(mgr); }
// enemy test build (src/enemy.inc): main+0xf98ff0 -> entry of f_80fb8ff0, comp = x0; F6 / F7 / F8 from the menu's key hook
void HsOnEnemyUpdate(uintptr_t comp) { OnEnemyUpdate(comp); }
// 2026-11-24: main+0x2c1d740 -> entry of f_82c3d740, actor = x0 (only while EnemyBusy)
bool HsEnemyBusy() { return EnemyBusy(); }
void HsOnActorUpdate(uintptr_t actor) { OnActorUpdate(actor); }
bool HsEnemyKey(int vk) { return EnemyKey(vk); }

// offline (sh hookshot/build.sh settings-test, no game): the settings text `in` (null = the defaults) through the
// reader and back out through the writer. Returns the lines that were not understood, -1 = out too small.
// menu preview: a few values changed and not saved
extern "C" __declspec(dllexport) void HsrdPreviewUnsaved() {
    g_tune.kick[2] = 22; g_cfg.force = 3; g_cfg.rag.mask = 0x40;
    snprintf(g_cfg.voicehard, sizeof g_cfg.voicehard, "VoiceDamageS,VoiceWallHit,Damage_S_01");
}
extern "C" __declspec(dllexport) int HsrdSettingsRoundTrip(const char* in, char* out, int cap) {
    g_cfg = Config(); g_tune = Tune();
    int bad = 0;
    if (in) {
        static char buf[CFG_TEXT_MAX];
        snprintf(buf, sizeof buf, "%s", in);
        bad = ReadConfigText(buf);
    }
    KeepAll();
    static char text[CFG_TEXT_MAX];
    int len = WriteConfigText(text, sizeof text);
    if (len + 1 > cap) return -1;
    memcpy(out, text, len + 1);
    return bad;
}
