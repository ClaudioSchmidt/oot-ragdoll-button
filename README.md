# Ragdoll Button

A button that turns Link into the game's own physics ragdoll in The Legend of Zelda: Ocarina of Time (Switch 2): from
any pose, keeping his speed.

| | |
|---|---|
| Version | 1.2.0 |
| Game | OoT (Switch 2) 1.1.1, title 0400486024DEC000 (other builds: the mod stays off) |
| Runs on | Windows and Linux (the PC ports Hookshot, Longshot, Triforce, ReLink), Switch / emulator |
| Needs | [HSML](https://github.com/ClaudioSchmidt/hsml) 2.1 on every platform |
| Download | [GameBanana](https://gamebanana.com/mods/740650) |
| License | GPL-2.0 (exlaunch) |

## Tested

Ragdoll Button 1.2.0 with HSML 2.1.0, 2026-11-26. Linux: in WSL with a software renderer.

| System | Port / emulator | Version | Mod loads | Menu in a save | Played |
|---|---|---|---|---|---|
| Windows | Hookshot | v2.3.2 | yes | yes | not yet |
| Windows | Triforce | v0.8.3 | yes | yes | not yet |
| Windows | ReLink | v0.7.9.4 | yes | yes | not yet |
| Windows | Longshot | v3.2.0 | yes | yes | not yet |
| Windows | Fritchii | v6.5 | yes | yes | not yet |
| Windows | Lichii | build-410 | yes | yes | not yet |
| Linux | Hookshot | v2.3.2 | yes | not yet | not yet |
| Linux | Triforce | v0.8.3 | yes | not yet | not yet |
| Linux | Longshot | v3.2.0 | yes | not yet | not yet |
| Switch 2 | hardware | | not tested | | |

## What it does

| Part | What |
|---|---|
| Ragdoll | the ragdoll button makes Link go limp from any pose; he keeps his speed |
| Kick | every further press while he is down kicks him again, in the direction of camera and left stick |
| Push | holding the button pushes him steadily |
| Spin and roll | left stick while down: spin in the air, log roll or drag on the ground |
| Get up | the get-up button; held 1.5 s: at once, at any speed |
| Force levels | Player, Light, Medium, Strong, Mighty: kick and push speed per level |
| Speed camera | the camera pulls back and the field of view widens with his speed |
| Voices | the game's voice clips per situation: press, falling, hard / light landing, get-up |
| Menu | a page in HSML's Mod Menu, built from HSML's menu kit |

## How it works

- Starts the game's own ragdoll on Link directly (no hit reaction) and keeps it running while the stand-up is vetoed;
  get-up releases the veto, moving his capsule to the body first when it lags behind.
- Kicks and the push write velocities into the ragdoll bodies after the game's physics op loop: per-body weights (core
  only, limbs trail), stacking kicks, lift on the ground, grip against sideways drift.
- Left stick while down: angular velocity in the air (spin), log roll / drag on the ground (grounded test: hips above
  the capsule + no vertical speed).
- Speed camera: distance and field of view from the body speed.
- Voices: the game's voice animations / sound keys per situation.
- Input: the npad read loop; the mod's buttons are hidden from the game during play only.
- Version check: fingerprints of the hook sites (`src/offsets.inc`); a mismatch keeps the mod off.
- One source: `src/` builds the Switch module (exlaunch hooks) and the PC DLL / `.so` (HSML detours on the same
  functions); `src/platform.hpp` is the only place that knows the difference.
- Settings: one table (`SETTINGS` in `src/config.inc`); the file's lines and the menu's rows come from it.

The game side (ragdoll states, bodies, offsets): [FINDINGS.md](FINDINGS.md).

## Controls

| Action | Default | Settings line |
|---|---|---|
| Ragdoll (held: push) | R tap | `ragdoll = r tap 0.40` |
| Get up (held 1.5 s: at once) | B tap | `getup = b tap 0.40` |
| Next force level | D-pad Down twice | `nextforce = down double 0.40` |
| Spin and roll (held) | none: the stick always spins | `spin_button = none` |
| This menu (straight to this page) | none | `menu = none double 0.40` |
| Menu | R3 twice / Insert (HSML's binding) | (HSML Settings, Controls) |

- Line format: `name = buttons trigger seconds`.
- Buttons: one or two of `a b x y l r zl zr lstick rstick plus minus left up right down` joined by `+`, or `none`.
- Trigger: `tap`, `double`, `hold`.
- PC keys: `ragkey`, `getupkey`, `forcekey`, `menukey`, `spinkey` (`none`, `a`-`z`, `0`-`9`, `f1`-`f12`, names, or `vk<hex>`).
- 1.1.0 files are read (`button`, `menubutton`, `setkey`).

| Force | Kick per press | Push while held |
|---|---|---|
| Player | 0 | 0 |
| Light | 6 m/s | 3 m/s |
| Medium | 32 m/s | 16 m/s |
| Strong | 80 m/s | 40 m/s |
| Mighty (default) | 150 m/s | 80 m/s |

## Files

| | Windows | Linux | Switch / emulator |
|---|---|---|---|
| Mod | `mods\Ragdoll Button\hsml\ragdoll-button.dll` | `mods/Ragdoll Button/hsml/ragdoll-button.so` | `ragdoll-button/exefs/subsdk7` in the game's mod folder |
| Settings | `mods\Ragdoll Button\oot_ragdoll.txt` | `mods/Ragdoll Button/oot_ragdoll.txt` | `sdmc:/OoT-Mods/oot_ragdoll.txt` |
| Log | `ragdoll.log`, `hsml.log` | the same | the emulator's log |
| HSML | `hsml\`, `mfplat.dll` | `hsml/` | the folder `HSML` beside the mod's, enabled |

fritchii's mod folder: `%APPDATA%\fritchii\load\0400486024DEC000\`.

## Layout

| Path | What |
|---|---|
| `src/main.cpp` | includes the parts below, in this order |
| `src/offsets.inc` | game addresses, fingerprints of the hook sites |
| `src/config.inc` | every setting and the `SETTINGS` table: file read / write and menu rows come from it |
| `src/state.inc` | state shared by the hooks, and which thread owns what |
| `src/voice.inc`, `src/settings.inc` | Link's voices; the settings file |
| `src/steer.inc`, `src/ragdoll.inc` | aim direction, spin, body velocity, "on the ground"; start, kicks, get-up |
| `src/input.inc`, `src/ai.inc`, `src/physics.inc`, `src/camera.inc` | the hooks' bodies: npad, AI thread, physics (kick / push / spin / roll, impact voice), speed camera |
| `src/enemy.inc` | enemy ragdolls: a test, PC only (Debug tab) |
| `src/menu.inc` | the menu page (Dear ImGui, HSML's kit `sdk/hsml_kit.hpp`) |
| `src/platform.hpp` | host access: guest memory, guest calls, log, ticks; on PC through `hookshot/platform_hs.hpp` |
| `src/hooks_switch.inc` | Switch: exlaunch hooks, the HSML client |
| `hookshot/` | PC (Windows and Linux): hook map (`hooks_hs.inc`), detours and lr filters (`hs_rt.cpp`), offline tests, menu preview, `mod.txt.in` |
| `tests/settings/` | settings files for the round-trip test |
| `release/` | the zips' texts, `pack.py` |
| `docs/`, `FINDINGS.md` | development notes, the game's ragdoll |

## Build

HSML's source beside this repo (`../hsml`); tools as in HSML's [CONTRIBUTING](https://github.com/ClaudioSchmidt/hsml/blob/main/docs/CONTRIBUTING.md).

```sh
git clone https://github.com/shadowninja108/exlaunch vendor/exlaunch && git -C vendor/exlaunch checkout f9f4b0d
git clone --depth 1 -b v1.91.9 https://github.com/ocornut/imgui vendor/imgui
sh build.sh                        # Switch (devkitPro devkitA64): out/ragdoll-button/exefs/subsdk7
sh hookshot/build.sh               # Windows (msys2 ucrt64 g++): out/hookshot/ragdoll-button.dll + mod.txt
sh hookshot/build.sh test          # offline: every hooked / called function against the port's exe
sh hookshot/build.sh settings-test # offline: defaults and tests/settings/*.txt read and written again
sh hookshot/build.sh menu-preview  # offline: menu tabs as PNGs
sh hookshot/build.sh linux         # Linux / WSL: out/linux/ragdoll-button.so + mod.txt; linux-test, linux-settings-test as above
python release/pack.py             # release zips: pc and switch; `python release/pack.py linux`: the Linux zip
```

## Credits

- [exlaunch](https://github.com/shadowninja108/exlaunch) by shadowninja108 (GPL-2.0)
- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut (MIT)
- Switch menu screen path from [Fast Travel](https://github.com/afiaka87/oots2-release) by HanSoloBlaster and afiaka87 (MIT): in the mod's own Switch menu up to 1.1.1; since then the menu, and that code, is HSML's
- Inspired by the Rampage Trainer for RDR2

Code written with Claude Opus 5.5 and Claude Fable 5.2 (Anthropic), including the reverse engineering; idea, design,
tuning and testing by ClaudioSchmidt. Breakdown on the [GameBanana page](https://gamebanana.com/mods/740650).
