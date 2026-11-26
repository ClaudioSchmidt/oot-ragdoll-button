#!/bin/sh
# Build the ragdoll-button exlaunch plugin -> out/ragdoll-button/exefs/subsdk7. Needs devkitPro (devkitA64).
# Usage: sh build.sh   (from Git Bash; runs make in devkitPro's own msys2 shell)
# Layout: src/ (main.cpp includes the *.inc parts), and exlaunch in vendor/exlaunch:
#   git clone https://github.com/shadowninja108/exlaunch vendor/exlaunch && git -C vendor/exlaunch checkout f9f4b0d
# The settings menu (src/menu.inc, -DRD_MENU) is a page in HSML's Mod Menu (../hsml, HSML_DIR to override): this module
# is a client that carries no menu (hsml switch/client/hsml_client.inc; hsml switch/DESIGN.md). The page draws with
# Dear ImGui v1.91.9 (vendor/imgui) configured by HSML's switch/nx_imconfig.h (the same file in every Switch mod: the
# page draws into the hosting module's context). Without the HSML module installed: no menu, the settings file works.
set -e
cd "$(dirname "$0")"
HSML="${HSML_DIR:-../hsml}"
# exlaunch's Makefile breaks on a path with spaces: build in the temp folder then
B=build/exl
case "$PWD" in *" "*) B="$(cygpath -u "${TEMP:-/tmp}")/oot-ragdoll-exl" ;; esac
rm -rf "$B" && mkdir -p build "$(dirname "$B")" && cp -r vendor/exlaunch "$B" && rm -rf "$B/.git"
P="$B/source/program"
cp src/*.cpp src/*.hpp src/*.inc "$P/"
I=vendor/imgui
cp $I/imgui.cpp $I/imgui_draw.cpp $I/imgui_tables.cpp $I/imgui_widgets.cpp $I/imgui_demo.cpp $I/imgui.h $I/imgui_internal.h $I/imstb_rectpack.h $I/imstb_textedit.h $I/imstb_truetype.h "$P/"
cp "$HSML/switch/nx_imconfig.h" "$P/imconfig.h"
# HSML's client side: hsml.h one folder above (hsml_nx.h and hsml_menu.hpp include "../hsml.h")
cp "$HSML/hsml.h" "$B/source/"
cp "$HSML/sdk/hsml_menu.cpp" "$HSML/sdk/hsml_menu.hpp" "$HSML/sdk/hsml_kit.hpp" "$HSML/switch/hsml_nx.h" "$HSML/switch/nx_fs.hpp" "$HSML/switch/client/hsml_client.inc" "$P/"
OUT=out/ragdoll-button/exefs/subsdk7
# 0.9.0 / 1.0.0 release: the build folder's absolute path (with the Windows user name) stays out of the binary's assert texts
MAP="-ffile-prefix-map=$(cygpath -m "$(cd "$B" && pwd)")=build"
/c/devkitPro/msys2/usr/bin/bash.exe -lc "export DEVKITPRO=/opt/devkitpro; cd '$(cygpath -m "$B")' && make -j8 LIBS=-lm CXX_FLAGS=\"$MAP -Wno-non-c-typedef-for-linkage -DRD_MENU -Wno-unused-function -Wno-error=array-bounds -Wno-error=format-truncation\""
mkdir -p "$(dirname "$OUT")"
cp "$B/deploy/subsdk9" "$OUT"
ls -l "$OUT"
