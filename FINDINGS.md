# oot-ragdoll: how Link's "huge hit" ragdoll is triggered

Game 0400486024DEC000 v2.0.0, main build id E0F3CCFAF50065AE. All addresses are `main+offset` (module offsets,
no 0x100 NSO header; add 0x100 for IPS). Static analysis only (2026-11-13); nothing confirmed live yet.
Offsets checked identical in Hero Mode's own `main` (only 71 words differ in the whole .text).

## Chain (damage -> ragdoll)

| Step | Where | What |
|---|---|---|
| 1 hit received | hit list of the player, `main+0x8d6320(player)` -> `+0x68` (entries 0x1d0 bytes, hash at +0x1c8, flags +0x95/+0x96) | attack info from the HitReaction system |
| 2 scheduler picks reaction | AINB `Player.scheduler.module` -> C++ selector `SelectorPlayerSchedulerReaction` (vtable `0x4b05c40`, choose fn `main+0x2b37270`) | picks child SmallHit/MediumHit/HugeHit...; child index bytes on the node: +0x27 SmallHit, +0x28 MediumHit, **+0x29 HugeHit**, +0x2a GuardBreak, +0x2b LandDamage (resolved by name in `main+0xd4c4e0`); selects via `main+0x200a20(node, idx)` |
| 3 action verb | `Player.scheduler.reactionhugehit.module.ainb`: `ExecuteBSASetActionVerb` (verb HugeHit) | |
| 4 action state | `Player.action.root.ainb` state `InterruptHugeHit` (player state enum 136 = 0x88, name table `0x4b4abc8`) -> `Player.action.reactionhugehit.module.ainb` | |
| 5 node | `ExecutePlayerReactionHugeHit` (vtable `0x49c79b0`), enter = `main+0x27fb9c0` | fills a request and calls step 6 |
| 6 **ragdoll start** | `main+0x3217550(ctrl, const Request*)` | ctrl = `main+0x5f88c0(main+0x837620(player))` |

The ragdoll data it selects: `Phive/Ragdoll/Reaction/Player_<Strength>_List` and
`Operation/Player_<Strength>_<Situation>_Operation` in `Pack/Actor/Player_Adult|Child.pack.zs`
(e.g. Rolling_Strong_OnGround: linear dir (0, 0.713, -1) speed 9, angular (-1,0,0) speed 10; local to the hit direction).

## Request struct (built on the stack in `0x27fbb48..0x27fbbe8`)

| Offset | Type | HugeHit value | Meaning |
|---|---|---|---|
| +0x00 | ptr | player actor | |
| +0x08 | f32 | 1.0 | passed as s0 to the final start call `main+0xef2040` |
| +0x0c/+0x10 | f32 | computed (yaw diff, `main+0x27fc050`) | passed as x3 to the start call |
| +0x14/+0x18 | f32 | 12.0 / 6.0 | |
| +0x1c/+0x20 | u32 | 1 / 2 | |
| +0x24 | u32 | node+0x20 | **strength**: 0 Rolling_Weak, 1 Rolling_Medium, 2 Rolling_Strong, 3 Rolling_RestraintThrown, 4 = none |
| +0x28 | u32 | node+0x24 | **situation**: 0 OnGround, 1 InAir, 2 InWater, 3 Climb, 4 Ride, 5 LikeLike, 6 Morpha, 7 BongoBongo |
| +0x2c | u32 | 0 | animation drive: 0 = "Default" |
| +0x30..+0x38 | Vec3 f32 | knock direction | **direction** (world). The ragdoll keeps only XZ, normalised, into ctrl+0x118..0x120 (Y=0). If X and Z are both 0 the old direction stays |
| +0x3c | u32 | 1 or 2 | 1 if \|yaw diff\| > 1.309 rad (75 deg), else 2 |

Strength in the node (`main+0x27fc7f0`): from the received-hit info `main+0x833e00(player)`: byte +0x2d2
(2 -> Strong, 3 -> Weak, else Medium; flag 0x4000 in +0x2b8 -> 4). Situation: `main+0x27fc8a0`.
Direction (`main+0x27fbd40(node, &mode, &outVec3, &outYaw)`): mode node+0x28; mode 0 = from the hit
(`main+0xa43aa0(actor, &vec, &yaw, 1)`), 1 = actor facing (`main+0xa43910` yaw -> `main+0xe63e80` vector),
2 = facing + pi, 3/4 = relative to an attacker position. Yaw -> unit vector: `main+0xe63e80`.

## Debugging in fritchii (`lichii.exe`, D:\...\Fritchii)

| Tool | State | Note |
|---|---|---|
| GDB stub | present | `[Debugging] use_gdbstub=false`, `gdbstub_port=6543` in `%APPDATA%\fritchii\config\qt-config.ini`; breakpoints + memory. Slows the JIT (memory checks); used before in lichii-memtools runs |
| NX2 debug server | present | env `EDEN_NX2_DEBUG_SERVER=host:port`, JSON lines over TCP: `status`, `read_memory`, `write_memory`, `log_tail`, `screenshot`... (source `lichii-memtools/src/core/debugger/nx2_debug_tools.cpp`). No breakpoints |
| Live run | not done | emulator was not running; needs you to play |

## Tools here

- `tools/unpack.py <ActorPack>` -> `work/<pack>/` (SARC + zstd with the game's dictionaries)
- `tools/byml.py <file.bgyml>` BYML v7 reader
- `tools/nso.py <string>...` string -> code/data xrefs (relocations applied); `tools/disasm.py <start> <end|+n>` annotated disassembly

## Live results (2026-11-13, GDB stub, main at 0x8033a000)

| Test | Result |
|---|---|
| Real hit | request via HugeHit enter (lr main+0x27fbbec): strength 1 (Medium), situation 0, +0c = hit angle vs facing (e.g. -0.07, -0.38, 1.72 rad), +3c 2, dir ~(-0.47, 0, -0.88) |
| Override strength 1 -> 2 | stronger fling |
| Override dir (flip) | no visible change: dir does not aim the fling |
| Override +0c += pi | Link flew toward the enemy: **+0c aims the fling** (relative to Link's facing; game: +0c = hitYaw - (actorYaw - pi)) |
| Direct call 0x3217550(ctrl, req) from ExecutePlayerWait update, no hit | full ragdoll (forward, Strong); Link stays down, the stick only jiggles him: no AI state to get up |
| Stop: main+0x3219090(ctrl, {actor, u8 0}) (what HugeHit's leave calls) | Link snaps back to standing |

Pointers seen this session: ctrl = 0x214ad26628, actor = 0x214a028d60 (change per load).
Tools: `tools/live.py` (log/override requests), `tools/fire.py` (call start/stop once), `tools/resume.py`.
GDB stub note: it does not step over breakpoints and stays halted after a client leaves: always `c` before closing.

## Get-up (2026-11-13, second session, main at 0x80ae2000)

- `rag.py down` (raw 0x3217550) + `rag.py up` (0x3219090 stop + player+0x1c3e |= 1, as the event node
  `ExecuteEventPlayerImmediateRagdollDown` does at main+0x26b42c0): Link snaps up, no animation.
  player+0x1c3c..0x1c48 are per-frame request flags (copied to +0x1c4c and cleared each frame, main+0x2e754b8);
  the main scheduler selector (main+0xf2ae00) only reads the get-up bit when byte +0x1c44 == 1.
- Natural get-up (watch2, real hit): `ExecutePlayerLDamageStandUp` enter main+0x27e4610 and
  `SelectorPlayerSchedulerLDamageStandUp` choose main+0x2b35ce0 run through the generic "keep current child"
  path (main+0x2b30ef0), not a fresh pick at main+0xf2affc.
- HugeHit update (main+0x27fc220) calls the ragdoll controller's per-frame update `main+0x321c040(ctrl, &args)`
  every frame; the raw call never ran that update, which is why Link stayed down and only jiggled.
  => use the game's own HugeHit reaction (scream, ragdoll update, get-up all included) instead of raw calls.

## Fake huge hit, power, hold, direction (2026-11-13, main at 0x80129000) — all confirmed live

| Piece | How | Result |
|---|---|---|
| Fake huge hit, no damage | on the Reaction selector's per-frame update (generic `main+0x2b30de0`, node vptr main+0x4b05c50) call `main+0x802000(node, node[+0x2a])` (HugeHit child; +0x29 = MediumHit = flinch) | full vanilla flow: HugeHit node -> ragdoll start (lr main+0x27fbbec) -> scream -> SlippingDown -> get-up animation |
| Power | request +0x08 (1.0 vanilla) at the ragdoll start | 1.0: 3.96 m, 3.0: 23.6 m, 0.0: collapses in place (<0.5 m) |
| Strength class | request +0x24 = 2 (Strong) | |
| Hold down | skip `ExecutePlayerSlippingDown` update `main+0x280ca50` (it runs the ragdoll update main+0x321c040 every frame) | Link stays down; on release he plays the get-up |
| Direction | request +0x0c = W - facing + pi; facing = atan2(actor[+0x330], actor[+0x348]) (= main+0xa43910); W = cameraYaw + stickAngle; cameraYaw = atan2(-c[+0x28], -c[+0x30]), c = [[[[main+0x4c7e888]]+0x248]+8]; stickAngle = atan2(-x, y) (player+0xe84, 0 = up; +0xe88 = last) | "away from camera" test flew straight away from the camera |

Other: Link position actor+0x31c (vec3), velocity actor+0x388 (vec3). Player object = [[actor+0x270]+0x358] (main+0x944e50).
Tools: `force_hit.py` (fake huge hit, env SCALE/PATCH/CHILD), `hold.py`, `measure.py`, `probe.py`, `dbg.py` (debug server).

## Core ragdoll ("limp, keep current speed") — confirmed live 2026-11-13 01:50

Recipe (all through the game's own reaction path, so lying phase + get-up animation are vanilla):
1. Read Link's velocity actor+0x388 and facing (atan2(actor[+0x330], actor[+0x348])).
2. Patch the loaded Weak list operations (entries OnGround/InWater/Ride; layout per entry: +0 u16 0xffff, u16 ApplyNumFrames,
   +4 Dir vec3, +0x10 Speed, +0x14 OperationSpace u32, +0x18 FixedDirection u8, +0x19 ApplyContinuous u8;
   angular block at +0x20 with the same layout, speed at +0x30): linear Dir (0, vy/|vh|, -1), Speed |vh|; angular Speed 0.
3. Force the Reaction selector to HugeHit (`0x802000(node, node[+0x2a])`).
4. In the HugeHit enter, right after `0x27fbd40` (main+0x27fbb00), set the hit yaw [x29-0x20] = facing: no turn.
5. At the ragdoll start (0x3217550) set +0x24 = 0 (Weak), +0x0c = moveYaw - facing + pi.
Result: he goes limp mid-motion, carries on forward, gets up with the real animation.

Warnings: my first oppatch.py restore wrote 4 bytes off and corrupted the Weak entries until repaired (01:44); the
measurements "speed 20 -> 16 m", "continuous -> 3.1 m", "up 1.6 m" ran on corrupted data and must be redone.
The raw call (0x3217550 without the AI path) also goes limp and keeps speed, but can't get up with animation
(the animation layer isn't in the lying pose; forcing LDamageStandUp leaves him down; stopping snaps).
Gentle alternatives tried and dropped: SlippingDown action (belly slide), DownLand list (slams down).

## Push / continuous force (2026-11-13 ~02:00, correct layout)

- One-shot vs continuous (Weak, speed 5.3, no spin, NOROT, away from camera): 1.64 m vs 3.31 m (ApplyContinuous=1, ApplyNumFrames=120).
- Operation values are read only at the reaction start: raising Speed while he lies does nothing (2nd test; the 1st "7 m" was something else).
- The push code: `main+0x38d5c90(op, container, bodyIdx, &worldDir, s0=scale)` adds scale*Speed*dir to
  `body[i]+0x90..0x98`; bodies = [container+0x68] (stride 0x138, count [container+0x60]); container seen = 0x214ac31428 (19 bodies).
- Consumer: per-frame ragdoll physics update `main+0x155c60` (loop at main+0x1584c4): if body+0x90 != 0 -> length -> body handle
  [body+0x38] (= +0x90-0x58) -> main+0x38bb040 ... applies velocity, then clears.
- Writing +0x90 from the debug server (0,6,0) right after a throw: no height change (2 tries). Open: the write may race with the
  clear, or the consumer runs only on the start frame. Next: watch writes/reads of body+0x90 per frame, or call the apply path
  (main+0x38bb040 with the body handle) from a hijack; or re-trigger the reaction for each push tick.
- Tools: force_hit.py env WATCH=<addr> (read watchpoint, logs readers incl. after the select), weak_set.py, weak_restore.py, fling.sh.
- Weak op entries (this session, main 0x80129000): hdr 0x214ac3cd40 / cdf0 / cea0; originals restored by weak_restore.py.

## Push while lying — solved (2026-11-13, main at 0x80129000)

- Ragdoll physics update `main+0x155c60` per frame: clears every body's `+0x90..+0x98` (linear) and `+0xa8..+0xb0` (angular)
  at `main+0x1579d4`, then runs the operation list (`[opset+0x28/+0x30]` and `[opset+0x38/+0x40]`, stride 0x1c;
  ApplyContinuous: while elapsed < ApplyNumFrames, else only on the first frame) which fills +0x90 via `main+0x38d5c90`,
  then the consumer loop at `main+0x1584c4` (x19 = container). That is why writes from outside did nothing: cleared first.
- Consumer per body with +0x90 = a != 0: n = a/|a|, v = linear velocity (`main+0x38bb040(handle=[body+0x38])`),
  new v = v - n(n.v) + |a| n + n(n.u), u = container+0xac; set with `main+0x12dbe0(handle, &v, 0)`. So +0x90 **sets** the
  velocity component along a to |a| (not additive). Angular: same at +0xa8 via `0x38bb130` / `0x12b790`.
- Live: breakpoint at 0x1584c4 (x19 == Link's container), write (0, 6, 0) into all 19 bodies' +0x90 each frame, SlippingDown
  update skipped: Link rose 11.4 m over 48 frames, then fell back. => plugin: hook at main+0x1584c4 (or right after the clear)
  and write the push vector there while X is held. Tool: `tools/push.py <base> <secs> vx vy vz [container] [actor]`.
- Not done: horizontal push test, throw-strength re-measure (the emulator hung, see below).
- Hang: `force_hit.py` stalled after the no-turn stop and was killed by `timeout 60` while the game thread was hijacked
  (lr = TRAP). The thread then sat at the ragdoll start; resuming it would run into the udf at TRAP. Needed an emulator
  restart. Never wrap force_hit.py in `timeout`; if it stalls, keep the TRAP breakpoint and restore the saved regs.
- Boot with gdbstub on: the guest waits for a client; after the first `c` rtld faults once (MOD0 probe, pc rtld+0xe34)
  and the stub halts again: send `c` a second time. Main this run: 0x80e4b000.
- Sideways push live (main 0x80e4b000, container 0x214b7f4428): (4, 0, 0) for 51 frames -> 8.8 m along +X. Lag while
  pushing = the GDB stops (2 per frame), not the game.

## Throw strength, clean Weak data (2026-11-13, main 0x80e4b000; one-shot linear op, no spin, forward, NOROT)

| Speed | Dir y (with z -1) | Horizontal after 5 s | Peak height |
|---|---|---|---|
| 0 | 0 | 0.21 m | +0.37 m |
| 5 | 0 | 2.03 m | +0.36 m |
| 10 | 0 | 2.66 m | +0.35 m |
| 20 | 0 | 5.93 m | +0.66 m |
| 10 | 1 (45 deg up) | 4.18 m | +1.95 m |
| 20 | 1 | 21.93 m | +4.63 m |
Flat throws die on ground friction; some upward part carries far. Tool: `tools/throwtest.py`. Op entries this run:
214b7ffd40/df0/ea0 (search_memory `ffff000000000000e0beee3e000080bf9a99a940`). Actor 0x214acdcbb8.

## Sprint-Button (subsdk8, exlaunch) — how it does input and menu
- 3 HookInline: `main+0x3469568` (input), `main+0x4ae798` (cb 0x8110), `main+0xc07f3c` (cb 0x8610); install fn ~0x8700
  fingerprints instructions first.
- Input (cb 0x7ba0): inside the game's npad read loop; [x29-0x18] = array of 16 NpadState (stride 0x28, +0 sampling no.,
  +8 buttons u64), x22 = npad slot (<= 8). It ORs the chosen button's new presses into its own global and **clears that bit
  from all 16 states** (bic) so the game never sees it. Button masks (nn::hid): none 0, B 2, L 0x40, R 0x80, LStick 0x10, RStick 0x20.
- Menu: rows added in the Options list (IDs, see oot-mod-compat README: 25/26 now); getter/setter keyed on row hashes;
  choices saved to sdmc:/OoT-Mods/oot_sprint.txt via nn::fs (MountSdCard, CreateFile/WriteFile/Flush).

## Plugin status + lying problem (2026-11-13 ~03:00) — parked, natural get-up for now

Plugin (`src/main.cpp`): press = limp + keeps speed + kick, hold = force; works in game. Staying down is OFF
(`HOLD_DOWN = false`, committed but NOT yet built/deployed at hand-over).
| Variant in SlippingDown update (main+0x280ca50) | Result |
|---|---|
| skip it all | stays down, but stiff: arms up, static |
| run only main+0x280cb80 (its ragdoll part) | fully limp, but stands up ~1 s after landing / when stuck against a wall while holding |
| send only the per-frame request (0x321c040, mode 2, like HugeHit) | stiff again |
=> limpness comes from main+0x3217e90 (called in 0x280cb80 when ctrl+0x17c is 1|3 and main+0x301afa0 is true): sets
ctrl+0x17c = 2 and switches the ragdoll operation to `DownLand`/`Default` (0x3218030, 0x3217850, 0x3217bc0).
Next time: let 0x3217e90 run (limp), then find what ends SlippingDown after it (watch ctrl+0x17c; the node's vtable
+0x158/+0x160 calls and timers self+0x30/+0x3c in 0x280ca50) and block only that.
Liveness: the push block (0x1584c4) stops running for him ~1 s after the last push; the plugin uses the controller
request 0x321c040 (inline hook) instead. Boot with gdbstub: `sh tools/attach.sh` (continues ~4x until fps > 0).

## Clean ragdoll + stand-up on demand — solved live (2026-11-13 ~09:50, main 0x80fe8000, tool `tools/live_rag.py`)

| Step | How | Result |
|---|---|---|
| Start | direct `main+0x3217550(ctrl, req)` (req: power 1, +0x0c = moveYaw - facing + pi, strength 0 Weak), no hit reaction | limp from any pose, keeps his speed, neutral face, no scream/arm flap |
| Then | same/next frame: main scheduler node (`SelectorPlayerSchedulerMain`, vtable main+0x4b05318, choose main+0xf2ae00) -> `main+0x802000(node, node[+0x3d])` = SlippingDown | eyes close, dust effect on ground impact, floats in water, player input blocked |
| Stay down | `SelectorPlayerSchedulerSlippingDown` (vtable main+0x4b094f8, slots +0x168/+0x170 -> `main+0x2b3d5f0`) picks the next child when SlippingDown ends (+0x21 Swim / +0x22 LDamageStandUp / +0x23 Fall); return 0 there = nothing new, he stays down | stays down limp indefinitely (fires ~1 s after landing) |
| Get up | stop vetoing | real stand-up animation |

Main scheduler child bytes (names resolved in main+0xd40860): +0x30 LDamageStandUp, +0x3d SlippingDown.
Without SlippingDown (raw start only): he jiggles on the ground, face/voice still react to input, sinks in water.

Push while lying in SlippingDown (live 09:57): the physics push block (main+0x1579b8, consumer 0x1584c4) only runs while the
container's operation set has ops (`[opset+0x28] > 0 || [opset+0x38] >= 1`, check at main+0x156494; else main+0x1564ac sets
container+0x18 bit 35 and skips). SlippingDown switches to an op set without ops -> +0x90 writes are never applied
(consumer hits 0/0.2 s while held; power +0xa0 = 1e-4 did not help). Fix: set the body velocities directly, consumer math:
n = a/|a| (`main+0xb0a0(vec*)` normalizes in place, returns |a|), v = `main+0x38bb040(handle=[body+0x38])` (vec3 in s0-s2),
u = container+0xac, v' = v - n(n.v) + |a| n + n(n.u), `main+0x12dbe0(handle, &v', 0)`; from main+0x1564b8 (x19 = container).

## Plugin state 2026-11-13 ~14:45 (commit 2e40920) — tested in game with Hero, Sprint, Graphics-Menu, compat on
| Input | Effect |
|---|---|
| ragdoll button (default R) press | clean ragdoll (direct start + SlippingDown), keeps speed + kick; while ragdolled every press = new kick |
| hold | push toward camera + left stick (body velocities set in PhysAfterHook, main+0x1564b8) |
| get-up button (default B; A if B is the ragdoll button) | stand-up animation; hidden from the game while down |
| menu button (default L3, `menubutton`) double-tap / hold 3 s, release + button | next force / rebind ragdoll button; saved to sdmc:/OoT-Mods/oot_ragdoll.txt |
Force levels (kick / push m/s): Player 0/0, Light 6/3, Medium 16/8, Strong 32/16, Mighty 100/50.
Raw buttons: the input hook re-calls the loop's GetNpadStates stub (by style tag [x29-0x1c]) because Sprint clears L3.
Conflict: GameBanana "Fast Travel" (oots2-release/fast-travel, MIT) also opens its menu on L3+R3 (our old combo, replaced by L3 double-tap/hold); it draws its own
menu with text/pictures: reference for on-screen feedback.
Open: lying-limp water/edge cases (voice/face + feedback: see "Link's voice" below).
Test tools: tools/live_rag.py (old plugin offsets), tools/buttons.py (button logger), tools/run.py (continue boot).

## Link's voice, face and sounds (2026-11-13, sessions with main at 0x80324000 / 0x80969000 / 0x809b9000)

| Piece | How | Note |
|---|---|---|
| AS slots | `[[[[actor+0x270]+0x40]+0x18]+0x18]`: count +0x18, array +0x20 (stride 0x48), `[elem]+0x10` = current AS name | slot 0 body, **4 face**, **6 voice**, 8 mask (`tools/slots.py` samples them) |
| Boulder hit (vanilla HugeHit) | slot 0 `DamageL` -> `SlippingDown`; slot 4 `FaceDamageL`, slot 6 **`VoiceDamageL`** (sound + open mouth); on landing slot 4 `FaceDown`; get-up slot 6 `VoiceDamageStand` | our clean ragdoll already gets `FaceDamageL` (from SlippingDown) but slot 6 stays empty -> closed mouth |
| Voice with mouth | `main+0xa29f90(playerObj, &{const char* asName})`, playerObj = `[[actor+0x270]+0x358]` (+0x18 actor, +0xfec face slot 4, +0xff0 voice slot 6) | what the AS 'Voice' event runs (enum -> name at main+0xa2e7d4, 116 names VoiceCutJumpSt..VoiceSwordEmpty in `Player_Adult.root.asb`); calls the generic slot request `main+0x2028f0`. Game picks the clip variant itself |
| Plain sound (no mouth) | SLink key on his sound user: user = `[[actor+0x270]+0x68]+0x78` (0x70 = ELink); lookup `main+0x43db00([[user+0x58]+0x18], u64* loc, key)` -> bool; emit `main+0x440630(user, &loc, &handle{u8 0xff,0,u16 0xffff,u32 0}, 0)` | = AINB `OneShotXLinkSearchAndEmit`. Keys = call-table names of user Player_Adult/Player_Child in `SLink2/slink2.Product.111.bslnk` (user hash crc32, name table at file+0x4d5dc8); voice keys = `Vo_PlAdult_<key>` assets: Damage_S/M/B_01-03, FallingScream_01/02, Jump_S_01-04, ... (adult 304, child 255, 192 shared) |
| Metal/effect keys | `Guard_Player` (shield clang), `GuardBreak_Player`, `EquipmentNoise_GuardBreak` (armor rattle), `Equip`, `UnEquip`, `SwordCharge_Level1/2`, `RotationCut_ChargeFull` | |

Plugin (this commit): press = voice tier by force level (+1 above 8 m/s), kick = tier per level, impact = body-0 speed drop > 6 m/s
in one physics frame (tiers from 10/25/60 m/s, 0.4 s gap, not while pushing); tiers voice1..4 in oot_ragdoll.txt;
force change = one shout per level (VoiceSideStep / AttackS / AttackM / AttackL / ChargeAttackFull), listening = armor rattle,
set = shield clang. Sounds queue (4) and play on the AI thread (selector update). Settings file has `version`; older
files get the voice defaults back. Live log: the newest of `eden_log.txt` / `eden_log.busy.txt` (fritchii alternates).
Tools: `audition.py` (play a list with gaps), `emit.py`, `voice_as.py`, `slots.py`, `watchslot.py` (who writes a slot),
`keylog.py`, `traceplay.py` (trace the plugin's PlayFromList).

## Other characters: who has a ragdoll, how enemies start one (2026-11-13, static only)

Scan of all 8373 `Pack/Actor/*.pack.zs`: 162 packs carry ragdoll files. A real rig needs `Phive/Ragdoll/Structure/*`;
packs with only `GameParameter/RagdollReactionParam` (Octorok, Poe, Leever, bosses...) have no rig.

| Group | Actors with a rig (Structure) | Reaction lists |
|---|---|---|
| Player rig | Player_Adult/Child/Base, Dm_Player_Child_Op, **Enemy_MiddleBoss_DarkLink** (CctComponentPlayerRagdollParam = Link's controller type) | Player_* |
| Biped enemies | Moblin, Lizalfos (+MiddleBoss), Dinolfos, Stalfos (all variants), Stalchild, ReDead, Gibdo, IronKnuckle (3 + Nabooru), SkullKid, Wolfos | FullDynamic (NumReactionFrames -1 = until stable), SmallDamage (20 frames, start from current pose); Stalfos ScatterBody; Wolfos Die |
| Small enemies | Keese (4), Bubble (5), Tektite (2), Tentacle (3), Skulltula (+Truth), Biri, DeadHand (+Hand), Dekubaba (4), Gohma_Larva, LikeLike, Peahat (2), TorchSlug, Tailpasaran, Poe_Big | FullDynamic / Die |
| NPCs | Npc_SkullKid_Child (3), Npc_SkullChild_001..005, Npc_SkullDad, Npc_Hylia_MSP_Default (Skulltula rig) | FullDynamic |
| No rig | every other NPC (Kokiri, Hylians, Zora, Goron, Gerudo...), Octorok, Poes, Leever, Dodongo, all dungeon bosses | - |
Full list (pack -> ragdoll files): `work/ragpacks.json`.

Enemies use a different controller than Link: component **`RagdollReactionController`** (game::component, vtable
main+0x4b4ed30, param `Component/RagdollReactionControllerParam/EnemyCommon`), at `[[actor+0x270]+0x390]`; comp+0x18 = actor.

| Piece | Where |
|---|---|
| AI module | `Common.action.ExecuteRagdollReaction.module` (in AI.Global pack), used by `EnemyCommon.common.ragdolldie`, `*.action.interruptdie`, `Bokoblin.action.interruptsmash`, `Lizalfos.action.Cower/InterruptRepelled`; inputs RagdollReactionParameter (`FullDynamic`), ASStartCommand (`SmashDamage`), ASDownWaitCommand (`DownWait`), ASDownRecoverCommand (`DownRecover`), DownTime... => death and "smash" knockdowns, with get-up (DownRecover) |
| C++ node | `ExecuteRagdollReactionProcess` vtable main+0x49cf400: enter main+0x28229a0 -> main+0x2b59800, update main+0x2822ae0 -> main+0x2b59cc0, leave main+0x2822c60 -> main+0x2b5a7a0 |
| **Start** | `main+0x2e916b0(comp, const Req*)`; callers: the node (0x2b59930), auto reaction (0x2e8f6dc), 0xf9a0ec |
| Is-ragdoll query | `main+0x2e94110(comp)` (comp+0x20 state: 0 none, 3 = ragdoll) |
| Auto reaction | `main+0x2e8f1f0(comp)` per frame from main+0xf99898 (enemy update); on an animation tag "Ragdoll" + flags it starts `FullDynamic` itself. Enabled by `IsEnableAutoRagdollReaction` (Lizalfos, Stalfos, ReDead...: `Enemy_Common_AutoRagdollReaction`) |

Request (0x38 bytes; auto-reaction values):
| Off | Type | Value | Meaning |
|---|---|---|---|
| +0x00 | char* | "FullDynamic" | reaction list name (node: RagdollReactionParameter) |
| +0x08 | char* | "" (main+0x3ee4f8c) | |
| +0x10 | char* | "" | forced AnimationDrive key ("KnockBack" -> "Common"/"Default" when state 1/3) |
| +0x18 | f32 | 0 | VelScale |
| +0x1c | vec3 | 0 | Rotation (direction of the operation) |
| +0x28 | u8 | 0 | node P+4 (IsForceDown?) |
| +0x2a | u8 | 1 | |
| +0x2c | i64 | -1 | |
| +0x34 | u32 | 0xffff | |

Enemy FullDynamic operation (Moblin): linear dir (0, 0.1, -2) speed 0.5 for 6 frames + body kicks: a collapse, not a fling.
Touch signals: no direct "Link touched X" callback found yet. Candidates: distance check in the per-frame controller
hook (comp -> actor pos vs Link pos); physics strings `CharacterUpdateInteractWithRagdoll`, `InteractHitImpulseScale`
(Link's capsule pushing a ragdolled body), filter `FilterRagdollCharacterAndPlayerRagdollBody`.

### Next mod / update: enemy ragdolls (parked 2026-11-13, user's wishes)
Not built. Townsfolk NPCs can't be ragdolled (no rig), so only enemies + the few NPCs above; it has to be a real
gameplay or fun addition, with **settings in a menu** (not sound feedback; the menu UI is known to the
oot-ragdoll / ragdoll-menu-compat sessions).
| Idea | Notes |
|---|---|
| Touch ragdoll | Link touches an enemy, or crashes into it while ragdolled -> enemy ragdolls. Proximity check in a hook on `main+0x2e8f1f0(comp)` (per frame per enemy) vs Link pos actor+0x31c, then `0x2e916b0`; push bodies like Link's kick |
| Posture bar | per-enemy meter filled by hits/blocks; full -> ragdoll knockdown (needs a damage hook + on-screen bar) |
| Ragdoll on every hit | option: each hit on an enemy starts FullDynamic (or SmallDamage list) |
| Open | more fun options; force/strength per option; get-up handling |
Risk for all: a start outside the enemy AI may never get up or fight the AI (as with Link). If so, drive the enemy's
own `Common.action.ExecuteRagdollReaction` path (DownWait/DownRecover). First live test: one raw start on a Moblin/Lizalfos.

## Snap-back on get-up (2026-11-13 ~17:50, main 0x808bd000 / 0x80347000; fixed)

| Piece | Finding |
|---|---|
| Actor position `actor+0x31c` | copied every frame from his physics capsule; only writer `main+0x1377c4` (via `main+0x136f34` <- `main+0x2c21740`, player physics update) |
| Capsule while ragdolled | chases the ragdoll at <= ~30 m/s and is blocked by terrain: 170 m behind after a Mighty kick; stood still 1.3 s while the body flew at 85 m/s |
| Get-up | the stand-up starts at the capsule -> "snaps back to where he last touched" |
| Body position | ragdoll body handle `[body+0x38]`: world matrix translation at handle +0x8c / +0x9c / +0xac (body 0 = hips) |
| Warp | `main+0x944e70(actor, const vec3*, 0)` (= `ExecuteSetActorPos` enter `main+0x283eb90`; also the safe-pos warp event): physics request `main+0x4c6630` on `[[actor+0x270]+0x50]`, else pending pos actor+0x358 |

Fix in the plugin: get-up press ignored while body 0 is faster than 4 m/s; when the capsule is > 2 m from the body,
warp it to the body on the AI thread (main selector update) and release the stand-up 3 AI frames later (warping in the
same frame showed him at the old spot first; warping on every get-up made the body hitch). Void-out/fall guard untouched:
he only moves to where the body already is. Impact voices now also while the push is held (compared with the velocity
the push set last frame; a continuous push into a surface counts once).
Open: a rare mid-ragdoll "short lag + snap to an earlier place" (user report) was not reproduced; the plugin logs
`snap:` lines (actor > 3 m or body > 4 m in one physics frame) to catch it. "IsCheckSafeRagdoll" etc. are
EnemyCommonParam (enemies), not Link.
Tools: `watchjump.py` (write-watch actor pos + single-step: hangs the stub at speed, don't use), `watchonce.py`
(one-shot write watch, N samples, no stepping: works).

## Force look, stick spin, speed camera (2026-11-13 ~18:00–19:45, main 0x80d37000; tested live)

| What | Finding |
|---|---|
| Live tuning | `struct Tune g_tune` in main.cpp, address logged as "tune @ 0x…"; `tools/tune.py field=value` writes it over the GDB stub (halt, M, continue). Layout: forceMode 0x4, w[32] 0x8, spinMode 0x88, spinKick 0x8c, spinPush 0x90, spinButton 0x98, logMap 0xa0, spinMax 0xa4, spinBrake 0xa8, groundGap 0xac, camDist 0xb0, camFov 0xb4, camLo 0xb8, camHi 0xbc, camIn 0xc0, camOut 0xc4, camLift 0xc8 (size 0xd0) |
| Per-body force | kicks/pushes set bodies 0–2 only (w = 1,1,1); the limbs follow through the joints, so he no longer moves as one rigid piece |
| Angular velocity | get `main+0x38bb130` (handle → vec3 in s0–s2), set `main+0x12b790(handle, &w, 0)`; whole-body spin = v += ω×(p−c), angular += ω for every body |
| Stick spin | the left stick both aims (camera yaw + stick) and rolls him toward the tilt: axis = (fz, 0, −fx) of that direction. Top speed 10 rad/s at full tilt (20 was too strong); a kick adds an 8 rad/s flip at Medium |
| Ground brake | stick released with hips < 0.6 m above the capsule (capsule within 1.5 m): angular × 0.85 per physics frame for 1 s; in the air the spin is kept |
| Camera | game camera view = `[[[[main+0x4c7e888]]+0x248]+8]` (eye floats +0x38, target +0x44, FOV +0x130). Its manager is applied by `main+0x4c48c0(mgr, …)` (called at main+0x20fb30, mgr = `[[x+0x270]+0x250]`); mgr +0xe8 eye, +0x110 target (doubles), +0x13c FOV (rad, default 0.8727 = 50°), `[[mgr+0x18]+8]` = view. Same function also runs for a UI camera (eye 0,0,0) |
| Speed camera | hook on main+0x4c48c0: for the game camera only, scale eye−target by 1 + camDist·k and add camFov·k degrees, call, restore the fields. k ramps 0→1 between 5 and 40 m/s body speed, 0.3 s in / 1 s out. Default distance 3.8 m → up to 7.6 m |
| Ground clip | pulling straight back put the camera into the ground behind him; now half goes up (camLift 0.5) and the eye never drops below the game's. Hills right behind can still clip (no collision check; accepted) |
| Body map | logMap=1 logs each body's offset from body 0 at physics frame 6; guess: chains 2–6 spine→head, 7–10, 12–14, 15–17 limbs, 11 unknown |

## 0.10.0 (2026-11-21, branch fun-control): built, NOT played yet

Written without the game running: both builds compile, offline tests green (hook test against hookshot.exe, settings
round trip incl. the two installed settings files, menu preview). Everything below "to check" is unverified in game.

| Change | Was (0.9.0) | Now | 0.9.0 value |
|---|---|---|---|
| Kick direction on the ground | camera pitch, usually downward = into the ground (cf. "Throw strength": flat throws die) | at least `kick_lift` 15 deg up; in the air unchanged | -90 |
| Push direction on the ground | camera pitch | at least `push_lift` 0 deg | -90 |
| Kick while already that fast | nothing (speed along the kick only raised to the kick speed) | adds `kick_stack` 25 % of the kick speed, up to `kick_stack_max` 2 kick speeds | 0 |
| Kick flip | + spinKick x kick/16 per kick, no limit (Mighty: 50 rad/s per kick) | up to `spin_kick_max` 30 rad/s | 1000 |
| Push | speed along the push only | + `push_grip`: 4 % of the sideways (horizontal) drift lost per frame | 0 |
| Ground roll | hips' rate measured, all bodies pushed the same about their centre; rate x sin(angle stick / spine) | every body driven to the rate about the spine line; full rate from 30 deg off the body axis; spine smoothed; "on the ground" held 12 frames | (not switchable) |
| Stick along his body on the ground | nothing | drag at `roll_along` 2 m/s | 0 |
| "On the ground" | hips low over the capsule | or: hips without vertical speed (< 1 m/s) for `rest_frames` 8 frames | 0 |
| Get-up while faster than 4 m/s | dropped | waits `getup_wait` 1.5 s; holding the button `getup_hold` 1.5 s gets him up at any speed | 0 / 0 |
| Ragdoll press outside play (game menu, cutscene) | fired when Link was back on his feet; button hidden from the game always | dropped after 1/3 s (4 s when pressed while he gets up); button hidden only in play | - |
| `voice1..4` in the file | never read (key pattern without digits) | read | - |
| Hookshot start kick | first op-apply call wrote it, later ops of that frame came on top | every op-apply call of the first frame writes it | - |
| Start kick when the push block never runs | Switch: no kicks in that ragdoll; Hookshot: written to +0x90 after the block (probably lost) | velocities set directly, 2 frames in | - |

To check in game first (state for each: third person, any force level unless named):
1. Medium, standing, camera level behind him: press R. Expect a low arc forward instead of a slide. Compare with
   `kick_lift = -90`.
2. Lying still, stick left / right across his body: he log-rolls that way, steadily, and stops rolling when the
   stick is let go. Stick toward his head / feet: he slides that way.
3. After a Strong flight (capsule far behind): the roll still works where he lies.
4. Mash R in one direction at Medium: each press adds speed, up to about 32 m/s.
5. Press B in mid-flight: he gets up by himself once he has come to rest (within 1.5 s), else nothing.
6. Open the game's own pause menu and press R there: nothing happens after closing it.
7. Hookshot: F9 opens the menu; a slider change is felt at once, its label turns yellow and the footer counts it;
   `oot_ragdoll.txt` has it after Save (not before). Voices tab, Link on his feet: the arrow next to a voice plays it.
   Start tab: Menu size resizes the menu when the slider is let go.
8. Hookshot with Immersive First Person installed too: both menus open (F9 / Home), neither breaks the other.

Stale notes above: "Plugin status" `HOLD_DOWN` no longer exists (the SlipNext veto does it); the Tune block is 0x130
bytes now (offsets in `src/config.inc`, `tools/tune.py`).

## 0.10.1 (2026-11-21, branch fun-control): stick switch; body turn and camera jumps were Immersive First Person's

User report, Triforce, ragdolled with force: (1) his body turns on the spot about up while the left stick moves, (2) the
camera follows in jumps. Both older than 0f23f15 (already reported 02:34 that day; 0f23f15 changes no physics /
camera code). Confirmed fixed in Triforce by the user.

| Finding | Evidence |
|---|---|
| The turn is not the ragdoll: Immersive First Person turned its drawn root bone under the physics body. SlippingDown is a roll rule there; with that action's camera on normal it was no special state, so its walk rule (moving = his drifting position + the stick) ran | hsfp.log `tpback … root -109 set 1 … as SlippingDown`, `face jump … as SlippingDown`; `rag stick` at the same time: tilt 0.56-1.01, hips 0.0 rad/s |
| The camera shown was Immersive First Person's third person view (its state file `first_person = 0` still means its own third person camera). Its wall factor from the game's hidden camera swung 0.05 .. 1 per flight, its lag snapped at 2 m | hsfp.log `tpwall … k 0.05 / 0.95 / 0.44`, `lag 1.72` |
| His capsule stays close to the body at up to ~30 m/s | `rag cam … gap to capsule 0.1-0.4 m` (a camera shift by body - capsule was tried and removed: it moved the hidden game camera by 2.3 m and made the jumps worse) |

Fixed in oot-first-person 1.12.120 (body left alone while ragdolled and getting up; lag 1:1 at speed; no wall sensor
while ragdolled; the floor holds only the height while he lies). Here: `stick_spin` (on by default) is the stick's own
switch; before, "Kicks flip him" (spin mode) off also stopped the stick. `log_rag = 1`: a `rag stick` line per 10
physics frames (tilt, stick spin, ground / air, hips' turn rate, speed). Tune: stickSpin 0x130, logRag 0x134, size 0x138.

## Enemy ragdolls: static scan + test build (2026-11-22, branch enemy-ragdoll)

Full scan (65 rig packs, AI.Global, every AS, damage tables): [docs/enemy-scan.md](docs/enemy-scan.md) (scripts and dumps
in work/scan/, not kept). Names only: AINB wiring was not parsed.

| Fact | Detail |
|---|---|
| No get-up after a ragdoll knockdown | No ASB in the game has `SmashDamage` / `DownWait` / `DownRecover`, which the smash path asks for. Guess: the enemy lies `GroundDownTime` frames (Lizalfos 80, most 30, ReDead / Gibdo 900), then the AI goes on with a snap |
| Real stand-up clips | IronKnuckle (`StandUp` + own module; its hit reaction runs the ragdoll node itself), DarkLink (Player-style), Dekubaba / Poe `GetUp` (not ragdoll) |
| Lizalfos / Dinolfos | auto reaction on, lists FullDynamic + SmallDamage, InterruptSmash wired but no hit maps to it (sword = InterruptSmallHit) |
| Moblin | damage table `NoReaction`: never reacts, ragdoll on death only |
| Damage tables | `romfs/System/CombinationDataTableData/game__life__ExternalDamageReactionTable_<name>.bgyml`: rows = damage type, columns None / Weak / Moderate / Strong / Smash; `InterruptSmash` = murmur3 3322560101 (Stalfos, Stalchild, Peahat on Spike / EnergyWall; DarkLink Smash column; Gohma_Larva) |
| Correction | `main+0x2e8f1f0(comp)` is not a per-frame call for every enemy: the controller update runs it only in state 1 / 3 (ragdoll running) |
| Controller update | `main+0xf98ff0(comp, ctx)` (Hookshot `f_80fb8ff0`): every actor with a RagdollReactionController, every frame, called from the actor update (main+0x2c1e3c0, `[sp+0x80]` = comp). Switch on comp+0x20: 2 -> its own path, 1 / 3 -> main+0x2e8f1f0, 0 -> auto checks. It starts ragdolls itself (main+0xf9a0ec) |
| Request as the auto reaction builds it (main+0x2e8f690) | +0x00 "FullDynamic" (main+0x3d3f3af), +0x08 / +0x10 "" (main+0x3ee4f8c), +0x18 f32 0, +0x1c vec3 0, +0x28 u8 0, +0x29 0, +0x2a 1, +0x2c 8 bytes -1, +0x34 u32 0xffff (size 0x38). "SmallDamage" = main+0x3d7f911 |

Test build 1.0.1-enemytest (Hookshot only, src/enemy.inc): a detour on the controller update notes every enemy (comp,
actor, tick); F6 / F7 / F8 (menu key hook, window thread) ask for a start; the next update of the nearest enemy within
30 m (F8: every one within 20 m) calls `main+0x2e916b0(comp, req)` with the request above (F7: SmallDamage), before
the original update. ragdoll.log `enemy ...` lines: the start (state before -> after), every state change with time and
how far the actor moved, a line per second, until state 0 or 60 s. Not run in game yet.
