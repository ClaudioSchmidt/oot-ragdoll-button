# Ragdoll Button (oot-ragdoll)

Press a button and Link goes limp: a full physics ragdoll from any pose, keeping his speed. Kick him again while he is
down, hold the button to push him around, and press B to get up. Ocarina of Time (Switch 2, game 1.1.1).

One source runs in two places:
- Switch / emulator: an exlaunch module (`exefs/subsdk7`), released as 0.9.0.
- Hookshot: a HSML mod with an in-game settings menu (branch `fun-control` = 0.10.0, on top of `hookshot`; not merged
  into master yet).

GPL-2.0 (exlaunch). Controls and settings: [release/README.txt](release/README.txt). How the game's ragdoll works:
[FINDINGS.md](FINDINGS.md).

## Users

**Switch / emulator**
- Install: copy `ragdoll-button` into the game's mod folder (fritchii:
  `%APPDATA%\fritchii\load\0400486024DEC000\ragdoll-button\exefs\subsdk7`).
- Turn off: set `enabled = 0` in the settings file, or delete the folder.
- Settings: `sdmc:/OoT-Mods/oot_ragdoll.txt`.
- Log: the emulator's log (`[oot_ragdoll]` lines).

**Hookshot** (v2.3.1)
- Requires HSML, its own download (`bin\dbghelp.dll`, see `../hsml`).
- Install: extract the zip into the Hookshot folder. That gives `mods\Ragdoll Button\hookshot\ragdoll-button.dll`.
- Turn off: rename the folder to `_Ragdoll Button`, or set `enabled = 0`.
- Uninstall: delete the folder.
- Settings: `mods\Ragdoll Button\oot_ragdoll.txt`, same keys as on the Switch.
- Menu: F9, or settings button (L3) + R3, in the style of Immersive First Person's: tabs Start, Force, Spin and roll,
  Get-up, Camera, Voices (lists with play / remove / add), Controls; a reset button per row; Menu size. A change works
  at once; **Save** writes the file (a yellow label and the footer show what is not saved yet). The settings button's
  own changes (force level, new ragdoll button) still save themselves. Menu off: an empty
  `mods\Ragdoll Button\no-overlay.txt`.
- Logs (next to hookshot.exe): `hsml.log` should say `running`, `ragdoll.log` should say `hooks installed` and, in
  game, `game version check: ok, running`. If there is no `hsml.log` at all, HSML is missing.
- Conflicts:
  - ReShade as `bin\dxgi.dll` crashes Hookshot on NVIDIA, and as `bin\dbghelp.dll` it would replace HSML.
  - OptiScaler is fine.
  - Immersive First Person hooks the same camera update (`f_804e48c0`); HSML chains the two detours.
  - Immersive First Person brings its own menu overlay (a second Vulkan layer, its own window and key). Both side by
    side: built for it (layer names differ, the environment lists are appended), **not run in game yet**.

## Developers

**Architecture**
- `src/main.cpp` is the game logic for both platforms, in parts (`src/*.inc`, listed at its top); hook bodies are
  `On*` functions.
- `src/config.inc` holds every value a player can change and one table (`SETTINGS`) that describes them: the settings
  file is read and written from it, the menu draws its rows from it. A new value = a field (in `Tune`: at the end,
  `tools/tune.py` knows the offsets) + one table line.
- `src/menu.inc` is the menu (Dear ImGui calls only). Its host on Hookshot is `hookshot/overlay.cpp` (the DLL is also a
  Vulkan layer; taken from Immersive First Person). The Switch has no host yet: `platform.hpp` stubs `Menu*`.
- `src/platform.hpp` is all host access: guest memory, `GCall` / `GCallV3`, `ToGuest`, log, ticks, settings file.
  On Hookshot it pulls in `hookshot/platform_hs.hpp`, which is built on `../hsml/sdk`.
- Where the hooks sit:
  - Switch: `src/hooks_switch.inc`.
  - Hookshot: `hookshot/hooks_hs.inc` (the hook map and the `Hs*` entry points) and `hookshot/hs_rt.cpp` (detours,
    lr filters, settings file).

**Build**
- Switch: `sh build.sh`. Needs devkitPro and exlaunch in `vendor/exlaunch` (commit f9f4b0d).
  - Output: `out/ragdoll-button/exefs/subsdk7`.
  - Release zips: `python release/pack.py`.
- Hookshot: needs msys2 ucrt64 g++, `../hsml` next to this repo, Dear ImGui v1.91.9 in `vendor/imgui` and the Vulkan
  SDK headers (see the top of `hookshot/build.sh`).
  - `sh ../hsml/build.sh` builds the loader.
  - `sh hookshot/build.sh` builds the mod: `out/hookshot/ragdoll-button.dll`.
  - Release zip: `python release/hookshot/pack.py` makes `release/ragdoll-button-hookshot-v<version>.zip` (the mod
    and the source; the loader ships as HSML's own zip).

**Porting a hook**: see "Porting a Switch hook" in `../hsml/README.md`. In this mod:
- The pad hooks sit on the forwarders `f_8566xxxx` and memset on `f_8572f300`, not on the import thunks
  `f_83c62xxx`: Hookshot's compiler calls the forwarders behind the thunks directly.
- Mid-function sites use lr filters (npad loop, op-apply call, the memset in the physics update).

Add a new site to the map in `hookshot/hooks_hs.inc` and to the list in `hookshot/build.sh test`.

**Testing**
- Offline, no game needed:
  - `sh hookshot/build.sh test` checks every hooked and called function against hookshot.exe.
  - `sh hookshot/build.sh settings-test`: the built DLL reads and writes the defaults and each file in
    `tests/settings/` (first line `# expect: bad N changed M`); a second pass must give the same text.
  - `sh hookshot/build.sh menu-preview [advanced 0|1]`: the menu's tabs as PNGs in `build/menu-preview/`.
- In game: install as above and read both logs.
- A Hookshot-only change must leave `subsdk7` the same: compare with `cmp -l` (the build id and one segment hash,
  72 bytes, differ whenever a source file's lines move).

## Platform differences

| Feature | Switch / emulator | Hookshot | Note |
|---|---|---|---|
| Loading | exlaunch module `subsdk7` | HSML mod DLL | |
| Button input | hook inside the npad loop (main+0x346957c) | exit of the 5 GetNpadStates forwarders, filtered by their return address in the loop | |
| Start kick | after the push-op loop, once | after each op-apply call of that frame (lr filter): the same write each time | if the push block never runs (empty op list), the physics hook sets the velocities two frames in, on both |
| Physics after | mid physics update (main+0x1564b8) | before the memset call in `f_80175c60` (lr filter), or after it returns when that call is skipped | |
| Version check | fingerprints of the hook sites, at start | the same fingerprints at the first hook call (guest memory isn't there yet at load); `versioncheck = off` skips it | checked offline against Hookshot's and Triforce's `main`: all 16 match |
| Settings menu | none yet | F9 / L3 + R3 (`overlay.cpp`) | |
| Settings file | `sdmc:/OoT-Mods/oot_ragdoll.txt` | `mods\<name>\oot_ragdoll.txt` | |
| Log | emulator log | `ragdoll.log`, `hsml.log` | |
| Release | zip (`tools/package.py`), 0.9.0 released | zip (`release/hookshot/pack.py`), needs HSML, 0.9.0 released | 0.10.0 not packed or released |

## What breaks when

| Change | Switch / emulator | Hookshot |
|---|---|---|
| Game update | fingerprints fail: the module refuses to install; needs new offsets | needs a new Hookshot build and new offsets. Missing `f_` names or failing fingerprints turn the mod off (`ragdoll.log`) |
| Hookshot update, same game | nothing | `f_` names usually stay. If one is gone or can't be hooked, `ragdoll.log` says so and the mod stays off |

## Open

- 0.10.0 / 0.10.1: played in Triforce on 2026-11-21 (ragdoll with Immersive First Person); the checklist in FINDINGS.md
  "0.10.0" is not walked through item by item. 0.10.1: FINDINGS.md "0.10.1".
- Immersive First Person's third person camera has no own collision (walls from the game's hidden camera, ground = a
  plane under his feet): its docs/open.md item 10, needs a GDB session.
- Menu on the Switch: needs a host (Immersive First Person paints its menu on the CPU there).
- One menu for several mods (a tab per mod) needs the overlay in HSML instead of in each mod.
- `sh hookshot/build.sh test` with `HOOKSHOT_EXE=...\Triforce.exe` crashes in HSML's test tool (the mod itself runs
  there).
