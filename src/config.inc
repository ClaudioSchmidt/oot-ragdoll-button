// oot-ragdoll, part of main.cpp: the values a player can change (Config, Tune) and the one table that describes them
// (SETTINGS): the settings file is read and written from it (settings.inc) and the Hookshot menu draws its rows from
// it (menu.inc). A new value = a field (Tune: at the end) + one line in the table.

// ---- settings -----------------------------------------------------------------------------------
// 1.1.1 (user: the controls as HSML's and Immersive Camera's): every action has one or two pad buttons (held together),
// a trigger (tap / double tap / hold) and a time (double tap: most time between the presses; hold: how long), plus a
// PC key (Hookshot). Masks: nn::hid bits, A 1, B 2, X 4, Y 8, L3 0x10, R3 0x20, L 0x40, R 0x80, ZL 0x100, ZR 0x200,
// Plus 0x400, Minus 0x800, d-pad left 0x1000, up 0x2000, right 0x4000, down 0x8000
enum Trig : u32 { TR_TAP, TR_DOUBLE, TR_HOLD };
struct Bind { u64 mask; u32 trig; float time; };
static_assert(sizeof(Bind) == 16);
struct Config {
    bool enabled = true;
    Bind rag = {0x80, TR_TAP, 0.4f};          // R: ragdoll, every further press a kick; held: the push
    Bind getup = {2, TR_TAP, 0.4f};           // B
    Bind nextforce = {0x8000, TR_DOUBLE, 0.4f};   // d-pad Down twice (1.1.0: the settings button's double tap)
    Bind menu = {0, TR_DOUBLE, 0.4f};         // none: the shared menu opens by its own binding (HSML's page); this one opens it at this mod
    int force = 4;      // (1.1.0 ship, user: Mighty; was Medium) 0 Player, 1 Light, 2 Medium, 3 Strong, 4 Mighty
    // Link's voice clips (SLink keys of Player_Adult/Player_Child, see FINDINGS.md); comma list = played in turn, none = off
    // names starting with "Voice" are his voice animations (sound + open mouth, game picks the clip), e.g. VoiceDamageL
    // 1.1.0: one list per situation (before: four lists by strength, shared by press, kick and impact)
    // 1.1.0 (user): press and kicks per force level [player, light, medium, strong, mighty]; landings by measured speed
    // 1.1.0 (user): the loud screams (VoiceFallDanger, FallingScream_*) only at Strong / Mighty, for the press and falling;
    // below that pain grunts, louder per level
    char voicepress[5][96] = {"VoiceDamageS", "VoiceDamageS,VoiceDamageM", "VoiceDamageM,VoiceDamageL",
                              "VoiceDamageL,VoiceDamageDanger,VoiceFallDanger", "VoiceFallDanger"};   // (1.1.0 ship, user: Mighty without the FallingScreams) every press of the ragdoll button (start and kicks)
    char voicefall[5][96] = {"none", "none", "none", "VoiceFallDanger", "VoiceFallDanger"};            // falling
    float fallspeed = 5.f;                                                     // falling voice: hips falling faster than this (m/s)
    float falldelay = 2.f;                                                     // ... for this long (s) before it plays
    float fallrepeat = 6.f;                                                    // ... and again every this many s while he keeps falling (0 = once)
    char voicegetup[160] = "VoiceDamageStand";                                 // get-up after a ragdoll (VoiceDamageStand = the game's own)
    char voicehard[160] = "VoiceDamageL,VoiceDamageLand,VoiceWallHit";         // comes down from the air hard (or a wall)
    char voicelight[160] = "VoiceDamageS";                                     // comes down from the air softly
    char voiceslide[160] = "none";                                             // bumps while already on the ground (dragged)
    bool impactvoice = true;                                      // voice when he hits ground/walls hard
    char forcevoice[160] = "VoiceSideStep,VoiceAttackS,VoiceAttackM,VoiceAttackL,VoiceChargeAttackFull";  // force change: entry per level
    // 0.10.0
    bool kickvoice = true;      // voice on the ragdoll press and on kicks
    bool versioncheck = true;   // the hook sites must be the known game build (off: only if a false alarm keeps the mod off)
    bool playonly = true;       // the mod takes its buttons only in play (not in the game's menus / cutscenes); off = always (0.9.0)
    int menukey = 0;            // Hookshot: PC key that opens the menu at this mod (Windows key code; 1.1.1: none, was F9)
    bool menublock = true;      // not used since the menu is HSML's (its "The controller works the menu"); kept so old files read
    bool advanced = false;      // Hookshot menu: the advanced rows show
    bool debugtab = false;      // Hookshot menu: the Debug tab shows
    int menusize = 150;         // Hookshot menu: size of its text and rows (percent; 150 = made for a 1080-row picture)
    // 1.1.0 (user): PC keys (Hookshot) that do what the pad buttons do; Windows key codes, 0 = none
    int ragkey = 0, getupkey = 0, spinkey = 0, forcekey = 0;   // (forcekey: 1.1.0's setkey)
};
constexpr int MENU_SIZE_MIN = 75, MENU_SIZE_MAX = 300, MENU_SIZE_STEP = 5;
Config g_cfg;
constexpr float IMPACT_HARD    = 10.f;                             // impact speed drop (m/s) from here: hard, below: light
constexpr u32 AIR_FRAMES       = 8;                                // physics frames (60/s) off the ground: "in the air"
constexpr float AIR_START_VY   = 3.f;                              // press while his vertical speed is above (m/s): in the air
constexpr float IMPACT_MIN     = 6.f;                              // body speed drop (m/s) in one physics frame = impact
constexpr float SPIN_REF_KICK  = 16.f;                             // spinKick is the flip of a kick of this speed (m/s)
// force look. Live-tunable: the Hookshot menu, or tools/tune.py over the GDB stub (address logged at start); saved in
// the settings file. tune.py knows the offsets: new fields go at the end.
struct Tune {
    u32 magic = 0x454e5554;   // "TUNE"
    u32 forceMode = 1;        // 0 = every body the same (rigid, 1.0.0), 1 = per-body weights w[] (limbs trail)
    float w[32] = {1, 1, 1};  // per body: share of kick/push speed (0 = left alone, follows through the joints)
    u32 spinMode = 1;         // 0 off, 1 auto (kick direction: forward = front flip), 2 random axis, 3 only with spinButton
    float spinKick = 8.f;     // rad/s added per kick of 16 m/s (scales with the kick speed)
    float spinPush = 3.f;     // rad/s added per physics frame while steering (spinButton + stick), up to spinMax
    u64 spinButton = 0;       // 0 = left stick always steers the spin while down; else only while this button is held
    u32 logMap = 0;           // 1 = log each body's offset from body 0 at the start (body-part map)
    float spinMax = 10.f;     // rad/s top spin while steering with the stick (full tilt)
    float spinBrake = 0.85f;  // stick released on the ground: spin kept per physics frame (1 = no brake), for 1 s
    float groundGap = 0.6f;   // "on the ground": hips at most this high above his capsule (m), capsule within 1.5 m
    float camDist = 1.0f;     // 0xb0 speed camera: extra eye-target distance at full speed (1 = twice as far, 0 = off)
    float camFov = 10.f;      // 0xb4 extra FOV at full speed (degrees)
    float camLo = 5.f;        // 0xb8 body speed (m/s) where the camera starts to pull back
    float camHi = 40.f;       // 0xbc body speed (m/s) for the full effect
    float camIn = 0.3f;       // 0xc0 seconds to follow a speed-up
    float camOut = 1.0f;      // 0xc4 seconds to come back after slowing down / getting up
    float camLift = 0.5f;     // 0xc8 share of the extra distance that goes up instead of back (the eye never drops below the game's)
    float rollMax = 6.f;      // 0xcc on the ground: top log-roll rate (rad/s, full tilt); spinMax is the air
    float rollRadius = 0.2f;  // 0xd0 on the ground: travel per radian of roll (m) = roll without slipping
    u32 headBody = 6;         // 0xd4 body index of his head/chest end of the spine (spine = body 0 -> this)
    float rollAlong = 2.f;    // 0xd8 on the ground, stick along his body: drag speed (m/s at full tilt), 0 = nothing (0.9.0)
    // ---- 0.10.0 ----
    float kick[5] = {0.f, 6.f, 32.f, 80.f, 150.f};  // (1.1.0 ship, user; was 0 6 16 32 100)  // 0xdc kick (m/s) per press and force level, on top of his own speed
    float push[5] = {0.f, 3.f, 16.f, 40.f, 80.f};  // (1.1.0 ship, user; was 0 3 8 16 50)   // 0xf0 steady speed while held, per force level
    float kickLift = 15.f;      // 0x104 a kick from the ground goes at least this far upward (degrees); -90 = where the camera looks (0.9.0)
    float pushLift = 0.f;       // 0x108 the push on the ground goes at least this far upward (degrees); -90 = where the camera looks (0.9.0)
    float kickStack = 0.25f;    // 0x10c a kick always adds at least this share of the kick speed (0 = nothing when he is already that fast, 0.9.0)
    float kickStackMax = 2.f;   // 0x110 ... while his speed along the kick is below this many kick speeds
    float spinKickMax = 30.f;   // 0x114 kicks flip him up to this rate (rad/s); 1000 = no limit (0.9.0)
    float pushGrip = 0.04f;     // 0x118 while pushing: share of his sideways drift lost per physics frame (0 = none, 0.9.0)
    u32 restFrames = 8;         // 0x11c also "on the ground": hips without vertical speed for this many physics frames (0 = capsule test only, 0.9.0)
    float getupSpeed = 4.f;     // 0x120 get-up only while his body is slower (m/s)
    float getupWait = 1.5f;     // 0x124 a get-up press while he is faster waits this long for him to slow down (s); 0 = dropped (0.9.0)
    float getupHold = 1.5f;     // 0x128 holding the get-up button this long gets him up at any speed (s); 0 = off (0.9.0)
    float warpGap = 2.f;        // 0x12c get-up: move the capsule to the body only if farther (m)
    // ---- 0.10.1 ----
    u32 stickSpin = 1;          // 0x130 the left stick spins him in the air and rolls / drags him on the ground (0.9.0 / 0.10.0:
                                //       always); 0 = the stick only aims kicks and the push (before 0.10.1 only spin mode off did that)
    u32 logRag = 0;             // 0x134 1 = log stick / spin every 10 physics frames while down
};
Tune g_tune;
static_assert(offsetof(Tune, rollAlong) == 0xd8 && offsetof(Tune, kick) == 0xdc && offsetof(Tune, warpGap) == 0x12c && offsetof(Tune, logRag) == 0x134 && sizeof(Tune) == 0x138,
              "tools/tune.py has these offsets");
const Config D_CFG;   // the defaults (reset, "changed" marks)
const Tune D_TUNE;

constexpr const char* MOD_VERSION = "1.1.1";
constexpr int CONFIG_VERSION = 10; // bump when voice defaults change: older files get them reset
constexpr struct { const char* n; u64 m; } BUTTONS[] = {{"none", 0}, {"a", 1}, {"b", 2}, {"x", 4}, {"y", 8},
    {"lstick", 0x10}, {"rstick", 0x20}, {"l", 0x40}, {"r", 0x80}, {"zl", 0x100}, {"zr", 0x200}, {"plus", 0x400},
    {"minus", 0x800}, {"left", 0x1000}, {"up", 0x2000}, {"right", 0x4000}, {"down", 0x8000}};
constexpr u64 BUTTON_BITS = 0xffff;   // every bit that has a name above (a rebind takes only these)
// PC keys (Hookshot) as buttons of their own, above every pad bit: held while their key is down (input.inc)
constexpr u64 KB_RAG = 1ull << 56, KB_GETUP = 1ull << 57, KB_SPIN = 1ull << 58, KB_FORCE = 1ull << 59;
constexpr const char* FORCES[] = {"player", "light", "medium", "strong", "mighty"};
// PC keys for the menu (Hookshot): name in the file -> Windows virtual-key code
constexpr struct { const char* n; int vk; } KEYS[] = {{"none", 0}, {"f1", 0x70}, {"f2", 0x71}, {"f3", 0x72}, {"f4", 0x73},
    {"f5", 0x74}, {"f6", 0x75}, {"f7", 0x76}, {"f8", 0x77}, {"f9", 0x78}, {"f10", 0x79}, {"f11", 0x7a}, {"f12", 0x7b},
    {"insert", 0x2d}, {"delete", 0x2e}, {"home", 0x24}, {"end", 0x23}, {"pageup", 0x21}, {"pagedown", 0x22}, {"pause", 0x13},
    {"space", 0x20}, {"tab", 0x09}, {"enter", 0x0d}, {"backspace", 0x08}, {"shift", 0x10}, {"ctrl", 0x11}, {"alt", 0x12},
    {"capslock", 0x14}, {"left", 0x25}, {"up", 0x26}, {"right", 0x27}, {"down", 0x28}, {"lshift", 0xa0}, {"rshift", 0xa1},
    {"lctrl", 0xa2}, {"rctrl", 0xa3}, {"lalt", 0xa4}, {"ralt", 0xa5}};   // + letters a-z, digits 0-9, any other as vk<hex>

// ---- the table -------------------------------------------------------------------------------
enum Kind : u8 {
    K_BOOL,     // bool            on / off
    K_FLOAT,    // float           shown as value * scale with the format in `unit`
    K_INT,      // u32
    K_CHOICE,   // u32             `unit` = the choices, each ending in \0
    K_BUTTON,   // u64 npad mask   names from BUTTONS, joined by + (one or two)
    K_BIND,     // Bind            buttons trigger seconds (1.1.1)
    K_FORCE,    // int             name from FORCES
    K_KEY,      // int             name from KEYS
    K_TEXT,     // char[cap]
    K_WEIGHTS,  // float[32]       comma list (file only)
};
enum Tab : u8 { T_FILE, T_START, T_FORCE, T_SPIN, T_GETUP, T_CAMERA, T_VOICE, T_CONTROLS, T_DEBUG, T_COUNT };
constexpr const char* TAB_NAMES[T_COUNT] = {"", "Start", "Force", "Spin and roll", "Get-up", "Camera", "Voices", "Controls", "Debug"};
struct Setting {
    const char* key;      // in the settings file
    Kind kind;
    void* p;              // the value, in g_cfg or g_tune
    Tab tab;              // menu tab; T_FILE = settings file only
    bool adv;             // menu: only with "Advanced"
    float lo, hi;         // menu range, in shown units
    float scale;          // shown = value * scale
    const char* unit;     // K_FLOAT / K_INT: printf format of the shown value; K_CHOICE: the choices
    const char* label;    // menu
    const char* help;     // menu (bottom line) and the comment in the file
    const char* section;  // menu: heading above this row
    u16 cap;              // K_TEXT: size of the buffer
};
constexpr float TURNS = 0.15915494f;   // rad/s -> turns per second
#define CF(f) &g_cfg.f
#define TU(f) &g_tune.f
#define TXT(f) K_TEXT, g_cfg.f
const Setting SETTINGS[] = {
    // key, kind, value, tab, adv, lo, hi, scale, unit, label, help, section, cap
    {"enabled", K_BOOL, CF(enabled), T_START, false, 0, 1, 1, nullptr, "Mod on", "Off: the ragdoll button does nothing and the game gets it back.", "Ragdoll Button"},
    {"playonly", K_BOOL, CF(playonly), T_START, true, 0, 1, 1, nullptr, "Buttons only in play", "On: in the game's menus and cutscenes the game keeps the ragdoll button, and a press there starts nothing later. Off: the mod always takes it (0.9.0)."},
    {"force", K_FORCE, CF(force), T_START, false, 0, 4, 1, nullptr, "Force level", "How hard the kicks and the push are. The speeds per level are on the Force tab."},
    {"advanced", K_BOOL, CF(advanced), T_FILE, false, 0, 1, 1, nullptr, "Advanced", "Shows the advanced rows of the menu."},
    {"debugtab", K_BOOL, CF(debugtab), T_FILE, false, 0, 1, 1, nullptr, "Debug tab", "Shows the Debug tab of the menu."},
    {"menusize", K_INT, CF(menusize), T_FILE, false, MENU_SIZE_MIN, MENU_SIZE_MAX, 1, "%d %%", "Menu size", "Size of the menu's text and rows, in percent (75 to 300)."},
    {"versioncheck", K_BOOL, CF(versioncheck), T_FILE, false, 0, 1, 1, nullptr, "Version check", "off = skip the check of the game build (only if a false alarm keeps the mod off)"},

    {"kick_player", K_FLOAT, TU(kick[0]), T_FORCE, false, 0, 150, 1, "%.0f m/s", "Player", "Speed a press adds in the aimed direction, on top of his own.", "Kick per press"},
    {"kick_light", K_FLOAT, TU(kick[1]), T_FORCE, false, 0, 150, 1, "%.0f m/s", "Light", "Speed a press adds in the aimed direction, on top of his own."},
    {"kick_medium", K_FLOAT, TU(kick[2]), T_FORCE, false, 0, 150, 1, "%.0f m/s", "Medium", "Speed a press adds in the aimed direction, on top of his own."},
    {"kick_strong", K_FLOAT, TU(kick[3]), T_FORCE, false, 0, 150, 1, "%.0f m/s", "Strong", "Speed a press adds in the aimed direction, on top of his own."},
    {"kick_mighty", K_FLOAT, TU(kick[4]), T_FORCE, false, 0, 150, 1, "%.0f m/s", "Mighty", "Speed a press adds in the aimed direction, on top of his own."},
    {"push_player", K_FLOAT, TU(push[0]), T_FORCE, false, 0, 80, 1, "%.0f m/s", "Player", "Speed he is kept at in the aimed direction while the button is held.", "Push while held"},
    {"push_light", K_FLOAT, TU(push[1]), T_FORCE, false, 0, 80, 1, "%.0f m/s", "Light", "Speed he is kept at in the aimed direction while the button is held."},
    {"push_medium", K_FLOAT, TU(push[2]), T_FORCE, false, 0, 80, 1, "%.0f m/s", "Medium", "Speed he is kept at in the aimed direction while the button is held."},
    {"push_strong", K_FLOAT, TU(push[3]), T_FORCE, false, 0, 80, 1, "%.0f m/s", "Strong", "Speed he is kept at in the aimed direction while the button is held."},
    {"push_mighty", K_FLOAT, TU(push[4]), T_FORCE, false, 0, 80, 1, "%.0f m/s", "Mighty", "Speed he is kept at in the aimed direction while the button is held."},
    {"kick_lift", K_FLOAT, TU(kickLift), T_FORCE, false, -90, 60, 1, "%.0f deg", "Kick from the ground goes up at least", "Kicks aim where the camera looks, usually a little downward. On the ground they go at least this far up. -90: camera only (0.9.0).", "Aim"},
    {"push_lift", K_FLOAT, TU(pushLift), T_FORCE, false, -90, 60, 1, "%.0f deg", "Push on the ground goes up at least", "0: the push slides him along the ground instead of into it. In the air it follows the camera. -90: camera only (0.9.0)."},
    {"kick_stack", K_FLOAT, TU(kickStack), T_FORCE, false, 0, 100, 100, "%.0f %%", "Kick again the same way adds", "Share of the kick speed a kick adds when he already flies that fast in that direction. 0: nothing (0.9.0)."},
    {"kick_stack_max", K_FLOAT, TU(kickStackMax), T_FORCE, true, 1, 6, 1, "%.1f x kick", "... up to a speed of", "Stacked kicks stop adding at this many kick speeds."},
    {"push_grip", K_FLOAT, TU(pushGrip), T_FORCE, false, 0, 30, 100, "%.0f %%", "Push grip", "While pushing: share of his sideways drift lost per physics frame. More = he follows the stick tighter. 0: none (0.9.0)."},
    {"limbs", K_CHOICE, TU(forceMode), T_FORCE, true, 0, 1, 1, "Whole body, rigid\0Core only, limbs trail\0", "Kicks and pushes move", "Core only: hips and spine are moved, arms and legs get dragged along."},
    {"weights", K_WEIGHTS, TU(w), T_FILE, true, 0, 1, 1, nullptr, "Body weights", "share of the kick / push per ragdoll body 0..23 (with limbs = 1)"},

    {"spin_mode", K_CHOICE, TU(spinMode), T_SPIN, false, 0, 3, 1, "Off\0Flip toward the kick\0Random flip\0Only with the spin button\0", "Kicks flip him", "What a kick does to his rotation. The left stick has its own switch below.", "Flips"},
    {"spin_kick", K_FLOAT, TU(spinKick), T_SPIN, false, 0, 5, TURNS, "%.2f turns/s", "Flip per kick", "Rotation a kick of 16 m/s adds; stronger kicks add more."},
    {"spin_kick_max", K_FLOAT, TU(spinKickMax), T_SPIN, false, 0, 20, TURNS, "%.1f turns/s", "Fastest flip from kicks", "Kicks stop adding rotation above this. 160 turns/s = no limit (0.9.0)."},
    {"stick_spin", K_CHOICE, TU(stickSpin), T_SPIN, false, 0, 1, 1, "Off\0On\0", "Left stick spins and rolls him", "On: the stick also spins him in the air and rolls him on the ground. Off: it only aims kicks and the push. Only while a button is held: Controls tab, Spin and roll.", "Left stick"},
    {"spin_max", K_FLOAT, TU(spinMax), T_SPIN, false, 0, 5, TURNS, "%.2f turns/s", "Top spin with the stick", "In the air, left stick at full tilt (stick spin on).", "Left stick in the air"},
    {"spin_push", K_FLOAT, TU(spinPush), T_SPIN, true, 0, 1.5f, TURNS, "%.2f turns/s", "Spin build-up per frame", "How fast the stick spins him up, per physics frame."},
    {"roll_max", K_FLOAT, TU(rollMax), T_SPIN, false, 0, 4, TURNS, "%.2f turns/s", "Top roll speed", "Stick across his body: log roll about his spine toward the stick, full from 30 degrees off his body axis.", "Left stick on the ground"},
    {"roll_radius", K_FLOAT, TU(rollRadius), T_SPIN, true, 0, 4, 6.2831853f, "%.2f m per turn", "Travel per roll", "Distance he covers per turn of the roll."},
    {"roll_along", K_FLOAT, TU(rollAlong), T_SPIN, false, 0, 6, 1, "%.1f m/s", "Drag along his body", "Stick along his body: drags him head or feet first at this speed. 0: nothing (0.9.0)."},
    {"spin_brake", K_FLOAT, TU(spinBrake), T_SPIN, true, 50, 100, 100, "%.0f %%", "Spin kept per frame after letting go", "On the ground, for 1 s after the stick is released. 100: no brake."},
    {"ground_gap", K_FLOAT, TU(groundGap), T_SPIN, true, 0, 3, 1, "%.2f m", "Grounded: hips above his capsule at most", "First test for \"he lies on the ground\" (roll instead of air spin)."},
    {"rest_frames", K_INT, TU(restFrames), T_SPIN, true, 0, 30, 1, "%d frames", "Grounded too: no vertical speed for", "Second test, for when his capsule is far away after a long flight. 0: off (0.9.0)."},
    {"head_body", K_INT, TU(headBody), T_SPIN, true, 0, 31, 1, "body %d", "Head end of his spine", "Ragdoll body that marks the head end (the roll axis runs from body 0 to it)."},

    {"getup_speed", K_FLOAT, TU(getupSpeed), T_GETUP, false, 1, 30, 1, "%.0f m/s", "He can get up when slower than", "While his body is faster, the get-up button waits.", "Get-up"},
    {"getup_wait", K_FLOAT, TU(getupWait), T_GETUP, false, 0, 5, 1, "%.1f s", "A press while faster waits", "The press is kept this long and he stands up as soon as he is slow enough. A kick cancels it. 0: the press is dropped (0.9.0)."},
    {"getup_hold", K_FLOAT, TU(getupHold), T_GETUP, false, 0, 5, 1, "%.1f s", "Holding the button gets him up anyway after", "For when he never comes to rest. 0: off (0.9.0)."},
    {"warp_gap", K_FLOAT, TU(warpGap), T_GETUP, true, 0, 20, 1, "%.1f m", "Stand up at the body when the game is farther behind", "The game's own position of Link lags behind the ragdoll; from this distance he is moved to the body first."},

    {"cam_dist", K_FLOAT, TU(camDist), T_CAMERA, false, 0, 300, 100, "+%.0f %%", "Camera pulls back by", "Extra distance at full speed. 0: off.", "Speed camera"},
    {"cam_fov", K_FLOAT, TU(camFov), T_CAMERA, false, 0, 40, 1, "+%.0f deg", "Field of view widens by", "At full speed."},
    {"cam_lo", K_FLOAT, TU(camLo), T_CAMERA, false, 0, 60, 1, "%.0f m/s", "Starts at", "Body speed where the effect begins."},
    {"cam_hi", K_FLOAT, TU(camHi), T_CAMERA, false, 5, 150, 1, "%.0f m/s", "Full at", "Body speed for the full effect."},
    {"cam_in", K_FLOAT, TU(camIn), T_CAMERA, true, 0, 3, 1, "%.2f s", "Follows a speed-up in", ""},
    {"cam_out", K_FLOAT, TU(camOut), T_CAMERA, true, 0, 5, 1, "%.2f s", "Comes back in", ""},
    {"cam_lift", K_FLOAT, TU(camLift), T_CAMERA, true, 0, 100, 100, "%.0f %%", "Share that goes up instead of back", "Up keeps the camera out of the ground behind him."},

    {"kickvoice", K_BOOL, CF(kickvoice), T_VOICE, false, 0, 1, 1, nullptr, "Voice on the ragdoll press and on kicks", "", "When"},
    {"impactvoice", K_BOOL, CF(impactvoice), T_VOICE, false, 0, 1, 1, nullptr, "Voice on landings and bumps", "When he hits the ground or a wall (the three lists below the push lists)."},
    {"fall_speed", K_FLOAT, CF(fallspeed), T_VOICE, false, 1, 30, 1, "%.0f m/s", "Falling voice when he falls faster than", "The Falling list plays when his hips fall faster than this (in the air, for the time below)."},
    {"fall_delay", K_FLOAT, CF(falldelay), T_VOICE, false, 0, 3, 1, "%.1f s", "Falling voice after", "How long he must keep falling before the Falling voice plays, counted from the last voice that played (a kick's scream starts it over)."},
    {"fall_repeat", K_FLOAT, CF(fallrepeat), T_VOICE, false, 0, 10, 1, "%.1f s", "Again while he keeps falling, every", "The Falling voice repeats this long after the last voice that played, while he keeps falling. 0 = once per fall."},
    {"voicepress_player", TXT(voicepress[0]), T_VOICE, false, 0, 0, 1, nullptr, "Ragdoll button", "Every press of the ragdoll button at this force level: the start and every kick, standing, lying or in the air.", "Voices per force level", sizeof(g_cfg.voicepress[0])},
    {"voicepress_light", TXT(voicepress[1]), T_VOICE, false, 0, 0, 1, nullptr, "Ragdoll button", "Every press of the ragdoll button at this force level: the start and every kick, standing, lying or in the air.", nullptr, sizeof(g_cfg.voicepress[1])},
    {"voicepress_medium", TXT(voicepress[2]), T_VOICE, false, 0, 0, 1, nullptr, "Ragdoll button", "Every press of the ragdoll button at this force level: the start and every kick, standing, lying or in the air.", nullptr, sizeof(g_cfg.voicepress[2])},
    {"voicepress_strong", TXT(voicepress[3]), T_VOICE, false, 0, 0, 1, nullptr, "Ragdoll button", "Every press of the ragdoll button at this force level: the start and every kick, standing, lying or in the air.", nullptr, sizeof(g_cfg.voicepress[3])},
    {"voicepress_mighty", TXT(voicepress[4]), T_VOICE, false, 0, 0, 1, nullptr, "Ragdoll button", "Every press of the ragdoll button at this force level: the start and every kick, standing, lying or in the air.", nullptr, sizeof(g_cfg.voicepress[4])},
    {"voicefall_player", TXT(voicefall[0]), T_VOICE, false, 0, 0, 1, nullptr, "Falling", "Once per flight, when he falls faster than the speed set above (again after a kick), at this force level.", nullptr, sizeof(g_cfg.voicefall[0])},
    {"voicefall_light", TXT(voicefall[1]), T_VOICE, false, 0, 0, 1, nullptr, "Falling", "Once per flight, when he falls faster than the speed set above (again after a kick), at this force level.", nullptr, sizeof(g_cfg.voicefall[1])},
    {"voicefall_medium", TXT(voicefall[2]), T_VOICE, false, 0, 0, 1, nullptr, "Falling", "Once per flight, when he falls faster than the speed set above (again after a kick), at this force level.", nullptr, sizeof(g_cfg.voicefall[2])},
    {"voicefall_strong", TXT(voicefall[3]), T_VOICE, false, 0, 0, 1, nullptr, "Falling", "Once per flight, when he falls faster than the speed set above (again after a kick), at this force level.", nullptr, sizeof(g_cfg.voicefall[3])},
    {"voicefall_mighty", TXT(voicefall[4]), T_VOICE, false, 0, 0, 1, nullptr, "Falling", "Once per flight, when he falls faster than the speed set above (again after a kick), at this force level.", nullptr, sizeof(g_cfg.voicefall[4])},
    {"voicegetup", TXT(voicegetup), T_VOICE, false, 0, 0, 1, nullptr, "Getting up", "Instead of the game's own voice when he stands up after a ragdoll. VoiceDamageStand = the game's own; empty = silent.", nullptr, sizeof(g_cfg.voicegetup)},
    {"voicehard", TXT(voicehard), T_VOICE, false, 0, 0, 1, nullptr, "Hard landing or wall hit", "He comes down from the air, or hits a wall in flight, at 10 m/s or more.", nullptr, sizeof(g_cfg.voicehard)},
    {"voicelight", TXT(voicelight), T_VOICE, false, 0, 0, 1, nullptr, "Light bump", "He comes down from the air, or hits a wall in flight, slower than that.", nullptr, sizeof(g_cfg.voicelight)},
    {"voiceslide", TXT(voiceslide), T_VOICE, false, 0, 0, 1, nullptr, "Sliding or dragged", "Bumps while he already lies on the ground: rolling, dragged along by the push.", nullptr, sizeof(g_cfg.voiceslide)},
    {"forcevoice", TXT(forcevoice), T_VOICE, true, 0, 0, 1, nullptr, "Force change", "One entry per force level (player, light, medium, strong, mighty), played when Next force level changes the level.", "Force change", sizeof(g_cfg.forcevoice)},

    {"ragdoll", K_BIND, CF(rag), T_CONTROLS, false, 0, 0, 1, nullptr, "Ragdoll", "Ragdoll, and a kick with every further press. Held: push him. The game does not get these buttons during play.", "Pad"},
    {"getup", K_BIND, CF(getup), T_CONTROLS, false, 0, 0, 1, nullptr, "Get up", "Stands him up. If it shares a button with Ragdoll, A does it (B when Ragdoll is A)."},
    {"nextforce", K_BIND, CF(nextforce), T_CONTROLS, false, 0, 0, 1, nullptr, "Next force level", "Player, Light, Medium, Strong, Mighty, then Player again. Saves itself."},
    {"menu", K_BIND, CF(menu), T_CONTROLS, false, 0, 0, 1, nullptr, "This menu", "Opens the shared menu at this mod's page, closes it there. none: the menu's own binding (HSML's page) still opens it."},
    {"spin_button", K_BUTTON, TU(spinButton), T_CONTROLS, false, 0, 0, 1, nullptr, "Spin and roll", "With stick spin on. none: the stick always spins and rolls him. A button: only while it is held, so the stick alone just aims. Kicks flip mode 'Only with the spin button': kicks flip only while it is held."},
    {"ragkey", K_KEY, CF(ragkey), T_CONTROLS, false, 0, 0, 1, nullptr, "Ragdoll key", "A PC key that does what the ragdoll buttons do (press, hold).", "PC keys"},
    {"getupkey", K_KEY, CF(getupkey), T_CONTROLS, false, 0, 0, 1, nullptr, "Get-up key", "A PC key that stands him up."},
    {"forcekey", K_KEY, CF(forcekey), T_CONTROLS, false, 0, 0, 1, nullptr, "Next force key", "A PC key for the next force level (with the trigger above)."},
    {"menukey", K_KEY, CF(menukey), T_CONTROLS, false, 0, 0, 1, nullptr, "Menu key", "Opens and closes the menu at this mod's page."},
    {"spinkey", K_KEY, CF(spinkey), T_CONTROLS, false, 0, 0, 1, nullptr, "Spin key", "A PC key that does what the spin buttons do."},
    {"menublock", K_BOOL, CF(menublock), T_FILE, false, 0, 1, 1, nullptr, "Pad controls the menu", "On: the pad moves through this menu and the game gets no input. Off: you keep playing and use the mouse here."},

    {"log_map", K_CHOICE, TU(logMap), T_DEBUG, false, 0, 1, 1, "Off\0On\0", "Log the body map at the next ragdoll", "Writes each ragdoll body's offset from body 0 to the log.", "Log"},
    {"log_rag", K_CHOICE, TU(logRag), T_DEBUG, false, 0, 1, 1, "Off\0On\0", "Log stick and spin while down", "Lines 'rag stick' in the log, about 6 a second."},
};
#undef CF
#undef TU
#undef TXT

// the same value in the defaults (these helpers serve the menu: unused in a build without it)
[[maybe_unused]] const void* DefaultOf(const Setting& s) {
    const char* p = static_cast<const char*>(s.p);
    const char* c = reinterpret_cast<const char*>(&g_cfg);
    if (p >= c && p < c + sizeof(g_cfg)) return reinterpret_cast<const char*>(&D_CFG) + (p - c);
    return reinterpret_cast<const char*>(&D_TUNE) + (p - reinterpret_cast<const char*>(&g_tune));
}
size_t SizeOf(const Setting& s) {
    switch (s.kind) {
        case K_BOOL: return sizeof(bool);
        case K_BUTTON: return sizeof(u64);
        case K_BIND: return sizeof(Bind);
        case K_TEXT: return s.cap;
        case K_WEIGHTS: return sizeof(g_tune.w);
        default: return 4;   // float, u32, int
    }
}
[[maybe_unused]] bool IsDefault(const Setting& s) {
    if (s.kind == K_TEXT) return !strcmp(static_cast<const char*>(s.p), static_cast<const char*>(DefaultOf(s)));
    return !memcmp(s.p, DefaultOf(s), SizeOf(s));
}
[[maybe_unused]] void ResetSetting(const Setting& s) { memcpy(s.p, DefaultOf(s), SizeOf(s)); }

// ---- what the settings file has ----
// The menu changes g_cfg / g_tune at once; the file gets them only with its Save button. g_saved* are the values the
// file has (or gets with the next write): the file is written from them, and a value that differs is "not saved yet".
// The settings button's own changes (force level, new ragdoll button) are kept one by one: they save themselves, as
// before, without taking the menu's unsaved changes along.
Config g_savedCfg;
Tune g_savedTune;
void* SavedOf(const Setting& s) {
    char* p = static_cast<char*>(s.p);
    char* c = reinterpret_cast<char*>(&g_cfg);
    if (p >= c && p < c + sizeof(g_cfg)) return reinterpret_cast<char*>(&g_savedCfg) + (p - c);
    return reinterpret_cast<char*>(&g_savedTune) + (p - reinterpret_cast<char*>(&g_tune));
}
[[maybe_unused]] bool IsSaved(const Setting& s) {
    if (s.kind == K_TEXT) return !strcmp(static_cast<const char*>(s.p), static_cast<const char*>(SavedOf(s)));
    return !memcmp(s.p, SavedOf(s), SizeOf(s));
}
void KeepAll() { memcpy(&g_savedCfg, &g_cfg, sizeof g_cfg); memcpy(&g_savedTune, &g_tune, sizeof g_tune); }
// one value (by its address in g_cfg / g_tune) into what the file gets
void KeepValue(const void* value) {
    for (auto& s : SETTINGS) if (s.p == value) memcpy(SavedOf(s), s.p, SizeOf(s));
}
[[maybe_unused]] int PendingCount() {
    int n = 0;
    for (auto& s : SETTINGS) if (!IsSaved(s)) n++;
    return n;
}
