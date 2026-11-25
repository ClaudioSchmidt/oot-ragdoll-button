#!/bin/sh
# Build the ragdoll-button exlaunch plugin -> out/ragdoll-button/exefs/subsdk7. Needs devkitPro (devkitA64).
# Usage: sh build.sh   (from Git Bash; runs make in devkitPro's own msys2 shell)
# Layout: src/ (main.cpp includes the *.inc parts), and exlaunch in vendor/exlaunch:
#   git clone https://github.com/shadowninja108/exlaunch vendor/exlaunch && git -C vendor/exlaunch checkout f9f4b0d
# The settings menu (src/menu.inc, -DRD_MENU) is a page in the Mod Menu from HSML (../hsml, HSML_DIR to override): its
# nx/ host, ui/ menu and painter and sdk/ mod side are built into this module (hsml nx/DESIGN.md), with Dear ImGui
# v1.91.9 (vendor/imgui) configured by HSML's nx/nx_imconfig.h (the same file in every Switch mod: the mods draw into
# one context).
# Test build: NX_MENU_VERSION=<n> sh build.sh carries menu version n (who hosts, hsml nx/hsml_nx.h) -> work/nx-test/
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
cp "$HSML/nx/nx_imconfig.h" "$P/imconfig.h"
# HSML's menu: hsml.h one folder above (menu.hpp and hsml_menu.hpp include "../hsml.h")
cp "$HSML/hsml.h" "$B/source/"
cp "$HSML/ui/menu.cpp" "$HSML/ui/menu.hpp" "$HSML/ui/ui_paint.cpp" "$HSML/ui/ui_paint.hpp" "$HSML/sdk/hsml_menu.cpp" "$HSML/sdk/hsml_menu.hpp" \
   "$HSML/nx/hsml_nx.h" "$HSML/nx/nx_fs.hpp" "$HSML/nx/modmenu_nx.inc" "$P/"
TEST=""; OUT=out/ragdoll-button/exefs/subsdk7
if [ -n "$NX_MENU_VERSION" ]; then TEST="-DHSML_NX_MENU_VERSION=$NX_MENU_VERSION"; OUT=work/nx-test/ragdoll-button-menu-v$NX_MENU_VERSION/exefs/subsdk7; fi
# 0.9.0 / 1.0.0 release: the build folder's absolute path (with the Windows user name) stays out of the binary's assert texts
MAP="-ffile-prefix-map=$(cygpath -m "$(cd "$B" && pwd)")=build"
/c/devkitPro/msys2/usr/bin/bash.exe -lc "export DEVKITPRO=/opt/devkitpro; cd '$(cygpath -m "$B")' && make -j8 LIBS=-lm CXX_FLAGS=\"$MAP -Wno-non-c-typedef-for-linkage -DRD_MENU -Wno-unused-function -Wno-error=array-bounds -Wno-error=format-truncation $TEST\""
mkdir -p "$(dirname "$OUT")"
cp "$B/deploy/subsdk9" "$OUT"
ls -l "$OUT"
