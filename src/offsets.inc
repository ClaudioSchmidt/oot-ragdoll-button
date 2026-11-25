// oot-ragdoll, part of main.cpp: main offsets and the fingerprints of the hook sites
// ---- main offsets -------------------------------------------------------------------------------
constexpr uintptr_t OFF_INPUT      = 0x346957c;  // inside the npad read loop (after Sprint's hook at 0x3469568)
constexpr uintptr_t OFF_CHOOSE     = 0x2b30de0;  // generic selector update (every AI node, every frame)
constexpr uintptr_t OFF_SELECT     = 0x802000;   // select child (node, idx)
constexpr uintptr_t OFF_REACT_VPTR = 0x4b05c50;  // SelectorPlayerSchedulerReaction node vtable (runs while he stands)
constexpr uintptr_t OFF_MAIN_VPTR  = 0x4b05318;  // SelectorPlayerSchedulerMain node vtable
constexpr u32 MAIN_SLIPPINGDOWN    = 0x3d;       // main node byte: child index of SlippingDown
constexpr uintptr_t OFF_SLIP_NEXT  = 0x2b3d5f0;  // SelectorPlayerSchedulerSlippingDown: pick the next child (stand up)
constexpr uintptr_t OFF_GET_CTRL   = 0x5f88c0;   // ragdoll controller from the player's physics object
constexpr uintptr_t OFF_START      = 0x3217550;  // ragdoll start (ctrl, request*)
constexpr uintptr_t OFF_CONSUMER   = 0x1584c4;   // physics push block: after the per-frame clear, before applying +0x90
constexpr uintptr_t OFF_PHYS_AFTER = 0x1564b8;   // physics update, after the push block (or after skipping it), x19 = container
constexpr uintptr_t OFF_RAGUPDATE  = 0x321c040;  // ragdoll controller per-frame update (ctrl, args): liveness
constexpr uintptr_t OFF_BODY_GETV  = 0x38bb040;  // body handle -> linear velocity (vec3 in s0-s2)
constexpr uintptr_t OFF_BODY_SETV  = 0x12dbe0;   // (body handle, const vec3*, 0): set linear velocity
constexpr uintptr_t OFF_BODY_GETW  = 0x38bb130;  // body handle -> angular velocity (vec3 in s0-s2)
constexpr uintptr_t OFF_BODY_SETW  = 0x12b790;   // (body handle, const vec3*, 0): set angular velocity
constexpr uintptr_t OFF_CAMERA     = 0x4c7e888;  // [[[[main+X]]+0x248]+8] = camera, +0x28 = back vector
constexpr uintptr_t OFF_CAM_APPLY  = 0x4c48c0;   // (camera manager, ...): eye/target/FOV -> view; mgr +0xe8 eye, +0x110 target (doubles), +0x13c FOV (rad); [[mgr+0x18]+8] = view
constexpr uintptr_t OFF_XL_LOOKUP  = 0x43db00;   // xlink search: (system=[[user+0x58]+0x18], u64* locator, const char* key) -> bool
constexpr uintptr_t OFF_XL_EMIT    = 0x440630;   // xlink emit: (user, const u64* locator, handle*, 0)
constexpr uintptr_t OFF_VOICE_AS   = 0xa29f90;   // player voice: (playerObj, const char** asName) -> AS on the voice slot (sound + mouth)
constexpr uintptr_t OFF_SET_POS    = 0x944e70;   // (actor, const vec3*, 0): warp actor + physics capsule (ExecuteSetActorPos)
// the npad loop's GetNpadStates stubs by style tag [x29-0x1c] (0..4); we call them again for the raw buttons, because
// Sprint-Button (hooked earlier in the same loop) clears its own button (L3) from the states we see
constexpr uintptr_t OFF_NPAD_STUBS[5] = {0x3c42800, 0x3c42830, 0x3c427f0, 0x3c42810, 0x3c42820};

struct Fingerprint { uintptr_t off; u32 word; };
// from the 2.0.0 main (tools/disasm.py); the plugin stays off if any differs
constexpr Fingerprint FINGERPRINTS[] = {
    {OFF_INPUT,      0xB85E43A9},  // ldur w9, [x29, #-0x1c]
    {OFF_CHOOSE,     0xD503237F},  // pacibsp (function start)
    {OFF_SELECT,     0xD503237F},
    {OFF_SLIP_NEXT,  0xD503237F},
    {OFF_START,      0xD503237F},
    {OFF_CONSUMER,   0xB9806268},  // ldrsw x8, [x19, #0x60]
    {OFF_PHYS_AFTER, 0xBD403660},  // ldr s0, [x19, #0x34]
    {OFF_RAGUPDATE,  0xA9402428},  // ldp x8, x9, [x1]
    {OFF_XL_LOOKUP,  0xD503237F},
    {OFF_XL_EMIT,    0xD503237F},
    {OFF_VOICE_AS,   0xD503237F},
    {OFF_SET_POS,    0xD503237F},
    {OFF_BODY_GETW,  0xD503237F},
    {OFF_BODY_SETW,  0xD503237F},
    {OFF_CAM_APPLY,  0xD503237F},
    {0x3c427f0,      0xB00083F0},  // adrp x16 (GetNpadStates stub)
};
