// oot-ragdoll, part of main.cpp: state shared by the hooks. Threads (from where the hooks sit; the game may run some
// of them on one thread): input = the npad loop (OnInput), AI = the selector update (OnChoose, OnSlipNext,
// OnRagUpdate), physics (OnConsumer, OnPhysAfter), camera (OnCamApply*); on Hookshot also the render and window
// threads (menu). Counters that cross threads are taken with Take (atomic), the rest are plain values where a
// stale read costs one frame at most.

// ---- input thread -> the others ----
volatile u64 g_slotButtons[9];
volatile u64 g_held;           // OR of all slots' newest buttons (before we hide ours)
volatile u64 g_heldPlay;       // the same for the ragdoll (push, spin button): 0 while the menu has the pad
volatile u32 g_pressPending;   // presses of the ragdoll button not yet used (Take)
volatile u64 g_pressTick;      // when the last one came: a press older than PRESS_FRESH is dropped
volatile u32 g_getupPending;   // presses of the get-up button not yet used (Take)
volatile u64 g_getupDownTick;  // since when the get-up button is held; 0 = not held
volatile s32 g_stickX, g_stickY;   // left stick for the ragdoll (centred while the menu has the pad)
volatile u64* volatile g_bindTarget;   // Hookshot menu: the next button pressed goes here (a button row is listening)
volatile u64 g_bindStart;              // when the menu's Set started recording (6 s without a press: given up)
volatile u64 g_bindDoneTick;           // when a button row took its button (the menu ignores the pad shortly after)
// ---- window thread (Hookshot) -> input thread ----
volatile u64 g_keyHeld;                // KB_* bits of the PC keys held now (OnInput adds them to the pad's buttons)
volatile int* volatile g_keyTarget;    // Hookshot menu: the next key pressed goes here (a key cell is listening)

inline u32 Take(volatile u32* n) { return __atomic_exchange_n(const_cast<u32*>(n), 0u, __ATOMIC_SEQ_CST); }
inline void Bump(volatile u32* n) { __atomic_fetch_add(const_cast<u32*>(n), 1u, __ATOMIC_SEQ_CST); }

// ---- the ragdoll ----
enum State { IDLE, ACTIVE };
volatile State g_state = IDLE;
volatile bool g_firstFrameDone;  // the start kick is in (physics)
volatile bool g_firstKickOpen;   // the push block may still (re)write the start kick: until the first OnPhysAfter after it
bool g_needSlip;                 // AI: select SlippingDown once after the start
volatile bool g_burstPending;    // a kick waits for the next physics frame
uintptr_t g_actor, g_ctrl;
float g_facing, g_vel[3];        // at the start: his yaw and velocity
float g_startDir[3];             // direction of the start kick (fixed at its first write)
volatile u64 g_lastActiveTick;   // last sign of life of the ragdoll (any hook)
volatile u64 g_standTick;        // last frame Link's Reaction selector ran = he is on his feet and playable
uintptr_t g_player;              // Link's actor, refreshed while he stands
float g_bodySpeed;               // speed of his first ragdoll body, last physics frame
float g_bodyPos[3];              // position of his first ragdoll body (hips), last physics frame
volatile u32 g_warpPending;      // get-up: 1 = move his capsule to g_warpPos (AI thread), 2..3 = wait, then stand up
float g_warpPos[3];
volatile u64 g_getupWantTick;    // a get-up press waits for him to slow down since this tick; 0 = none
u32 g_physFrames;                // physics frames since the ragdoll start
u32 g_groundHold;                // physics frames "on the ground" still holds after the tests said no (OnPhysAfter)
u32 g_airFrames;                 // physics frames off the ground (Grounded with its hold); 0 after 4 frames on it
u64 g_fallSince;                 // tick he began to fall fast (0 = not falling)
u64 g_fallLast;                  // tick of the last falling voice in this fall (0 = none yet)
volatile u64 g_getupTick;        // GetUp: the game's get-up voice in the next seconds is ours to replace
volatile bool g_ownVoice;        // our own call of the voice function (the voice hook lets it pass)
u32 g_groundRun;                 // physics frames in a row on the ground
u32 g_restFrames;                // physics frames in a row (with hysteresis) without vertical speed of his hips
bool g_spinPending;              // kick/start: add a spin on the next physics frame
float g_spinScale;               // start kick speed / SPIN_REF_KICK
u32 g_startWait;                 // physics frames the start kick waited for the push block
float g_spine[2]; bool g_spineOk; // ground roll: his smoothed spine direction (hips -> head, x and z)
u32 g_steerFrames, g_sinceSteer = 999;   // ground brake: frames the stick steered / since it let go
float g_lastDrop;                // impact voice: last frame's speed change of body 0
Vec3 g_impactPrev; uintptr_t g_impactPrevCont;  // body 0 velocity after last physics frame (impact voice)
Vec3 g_snapActor, g_snapBody;    // snap log: last frame's positions

// ---- voice requests from any thread; played on the AI thread (VoiceTick) ----
struct SayItem { const char* list; };
SayItem g_sayQ[4];
volatile u32 g_sayHead, g_sayTail;
volatile u8 g_sayLock;           // Say runs on several threads
volatile u64 g_lastPlayTick;     // last voice actually played (AI thread): the falling voice counts its delay from it
volatile u64 g_lastSayTick;      // last voice request (impact voices wait 0.4 s after any voice)

// ---- settings file ----
bool g_configLoaded;
volatile u64 g_saveTick;         // a write of the settings file was asked for at this tick: done half a second later; 0 = none
volatile u8 g_saveLock;
inline void RequestSave() { u64 t = Ticks(); g_saveTick = t ? t : 1; }
