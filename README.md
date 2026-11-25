# Ragdoll Button

A button that turns Link into the game's own physics ragdoll in The Legend of Zelda: Ocarina of Time (Switch 2): from
any pose, keeping his speed. Kick him while he is down, push him with the camera and left stick, steer spins and rolls,
get up. One source builds an exlaunch module for Switch / emulators and an [HSML](https://github.com/ClaudioSchmidt/hsml)
mod for the Hookshot-based PC ports.

| | |
|---|---|
| Version | 1.1.1 |
| Game | OoT (Switch 2) 1.1.1, title 0400486024DEC000 (other builds: the mod stays off) |
| Runs on | Switch / emulator (`exefs/subsdk7`), PC ports Hookshot, Longshot, Triforce, ReLink (needs HSML 2.0+) |
| Download | [GameBanana](https://gamebanana.com/mods/740650) |
| License | GPL-2.0 (exlaunch) |

## How it works

- Starts the game's own ragdoll on Link directly (no hit reaction) and keeps it running while the stand-up is vetoed;
  get-up releases the veto, moving his capsule to the body first when it lags behind.
- Kicks and the push write velocities into the ragdoll bodies after the game's physics op loop: per-body weights (core
  only, limbs trail), stacking kicks, lift on the ground, grip against sideways drift.
- Left stick while down: angular velocity in the air (spin), log roll / drag on the ground (grounded test: hips above
  the capsule + no vertical speed).
- Speed camera: distance and field of view from the body speed.
- Voices: the game's voice animations / sound keys per situation (press, falling, hard / light landing, slide, get-up).
- Input: the npad read loop; the mod's buttons are hidden from the game during play only.

Details of the game side (ragdoll states, bodies, offsets): [FINDINGS.md](FINDINGS.md).

## Controls

| Action | Default | Settings line |
|---|---|---|
| Ragdoll (held: push) | R tap | `ragdoll = r tap 0.40` |
| Get up (held 1.5 s: at once) | B tap | `getup = b tap 0.40` |
| Next force level | D-pad Down twice | `nextforce = down double 0.40` |
| Spin and roll (held) | none: the stick always spins | `spin_button = none` |
| This menu (straight to this page) | none | `menu = none double 0.40` |
| Menu | R3 twice / Insert (HSML's binding) | (HSML: `hsml\hsml.txt`) |

Line format: `name = buttons trigger seconds`; buttons one or two of `a b x y l r zl zr lstick rstick plus minus left up
right down` joined by `+`, or `none`; trigger `tap` / `double` / `hold`. PC keys: `ragkey`, `getupkey`, `forcekey`,
`menukey`, `spinkey` (`none`, `a`-`z`, `0`-`9`, `f1`-`f12`, names, or `vk<hex>`). 1.1.0 files are read
(`button`, `menubutton`, `setkey`).

| Force | Kick per press | Push while held |
|---|---|---|
| Player | 0 | 0 |
| Light | 6 m/s | 3 m/s |
| Medium | 32 m/s | 16 m/s |
| Strong | 80 m/s | 40 m/s |
| Mighty (default) | 150 m/s | 80 m/s |

## Files

| | Switch / emulator | PC |
|---|---|---|
| Mod | `ragdoll-button/exefs/subsdk7` in the game's mod folder (fritchii: `%APPDATA%\fritchii\load\0400486024DEC000\`) | `mods\Ragdoll Button\hsml\ragdoll-button.dll` |
| Settings | `sdmc:/OoT-Mods/oot_ragdoll.txt` | `mods\Ragdoll Button\oot_ragdoll.txt` |
| Log | emulator log | `ragdoll.log`, `hsml.log` |

## Layout

| Path | What |
|---|---|
| `src/main.cpp`, `src/*.inc` | game logic for both platforms: ragdoll, physics, steer, camera, voice, input, settings |
| `src/config.inc` | every setting and the `SETTINGS` table: file read / write and menu rows come from it |
| `src/menu.inc` | the menu page (Dear ImGui) |
| `src/platform.hpp` | host access: guest memory, guest calls, log, ticks; on PC through `hookshot/platform_hs.hpp` |
| `src/hooks_switch.inc` | exlaunch hooks, version fingerprints, Mod Menu registration |
| `hookshot/` | HSML side: hook map (`hooks_hs.inc`), detours and lr filters (`hs_rt.cpp`), offline tests, menu preview |
| `tests/settings/` | settings files for the round-trip test |
| `docs/`, `FINDINGS.md` | development notes, the game's ragdoll |

## Build

```sh
git clone https://github.com/shadowninja108/exlaunch vendor/exlaunch && git -C vendor/exlaunch checkout f9f4b0d
git clone --depth 1 -b v1.91.9 https://github.com/ocornut/imgui vendor/imgui
# HSML next to this repo: ../hsml
sh build.sh                        # Switch (devkitPro devkitA64): out/ragdoll-button/exefs/subsdk7
sh hookshot/build.sh               # PC (msys2 ucrt64 g++): out/hookshot/ragdoll-button.dll + mod.txt
sh hookshot/build.sh test          # offline: every hooked / called function against the port's exe
sh hookshot/build.sh settings-test # offline: defaults and tests/settings/*.txt read and written again
sh hookshot/build.sh menu-preview  # offline: menu tabs as PNGs
python release/pack.py             # release zips (pc, switch)
```

## Credits

- [exlaunch](https://github.com/shadowninja108/exlaunch) by shadowninja108 (GPL-2.0)
- [Dear ImGui](https://github.com/ocornut/imgui) by Omar Cornut (MIT)
- Switch menu screen path from [Fast Travel](https://github.com/afiaka87/oots2-release) by HanSoloBlaster and afiaka87 (MIT)
- Inspired by the Rampage Trainer for RDR2

Code written with Claude Opus 5.5 and Claude Fable 5.2 (Anthropic), including the reverse engineering; idea, design,
tuning and testing by Clausioporosis. Breakdown on the [GameBanana page](https://gamebanana.com/mods/740650).
