#!/bin/sh
# Hookshot build (msys2 ucrt64 g++): out/hookshot/ragdoll-button.dll = this mod (the same src/main.cpp as the Switch
# build, -DHOOKSHOT -DRD_MENU); goes to <Hookshot>\mods\Ragdoll Button\hsml\. Built on HSML (../../hsml, HSML_DIR to
# override): its loader is its own download (sh ../../hsml/build.sh -> hsml/out/).
# The settings menu (src/menu.inc) is a page in HSML's shared menu (HSML 2.0, hsml-ui.dll): this DLL links Dear ImGui
# itself, the version and settings HSML's menu was built with (v1.91.9 in ../vendor/imgui, HSML's sdk/hsml_imconfig.h),
# and draws into the menu's context. No Vulkan layer and no window of its own any more.
#   git clone --depth 1 -b v1.91.9 https://github.com/ocornut/imgui ../vendor/imgui
# Tests, all offline (no game needed):
#   sh build.sh test             against hookshot.exe: the functions this mod hooks and calls
#   sh build.sh settings-test    the built DLL reads and writes the settings (defaults + ../tests/settings/*.txt)
#   sh build.sh menu-preview [advanced 0|1] [w] [h] [scale]   the menu's tabs as PNGs in ../build/menu-preview
set -e
cd "$(dirname "$0")"
export PATH=/c/msys64/ucrt64/bin:$PATH
HSML="${HSML_DIR:-../../hsml}"
FLAGS="-O2 -std=c++17 -Wall -Wno-unused-function -Wno-format"
LINK="-shared -static -static-libgcc -static-libstdc++"
DLL=../out/hookshot/ragdoll-button.dll
if [ "$1" = test ]; then
    # h = hooked (hs_rt.cpp: choose, slipnext, ragupdate, camapply, voice, then consumer, physafter, memset and the 5 npad
    # forwarders by guest address), then the game functions main.cpp calls (GCall)
    M=0x80020000
    exec sh "$HSML/build.sh" test $(for o in 0x2b30de0 0x2b3d5f0 0x321c040 0x4c48c0 0xf98ff0 0x2c1d740 0xa29f90; do printf 'h%x ' $((M + o)); done) \
        h838f5b70 h80175c60 h8572f300 h8566b410 h8566ac30 h8566b500 h8566b5f0 h8566ad20 \
        $(for o in 0x802000 0x5f88c0 0x3217550 0x38bb040 0x12dbe0 0x38bb130 0x12b790 0x43db00 0x440630 0xa29f90 0x944e70 0x2e916b0 \
                   0x3c42800 0x3c42830 0x3c427f0 0x3c42810 0x3c42820; do printf '%x ' $((M + o)); done)
fi
if [ "$1" = settings-test ] || [ "$1" = menu-preview ]; then
    mkdir -p ../build
    g++ -O1 -std=c++17 -static -o ../build/offline_test.exe offline_test.cpp
    if [ "$1" = settings-test ]; then exec ../build/offline_test.exe settings $DLL ../tests/settings/*.txt; fi
    mkdir -p ../build/menu-preview
    exec ../build/offline_test.exe menu $DLL ../build/menu-preview "${2:-0}" "${3:-1280}" "${4:-1000}" "${5:-1.5}"
fi
mkdir -p ../out/hookshot
IMGUI="${IMGUI_DIR:-../vendor/imgui}"
IMFLAGS="-include $HSML/sdk/hsml_imconfig.h -I$IMGUI"
OBJ=../build/hsml-imgui; mkdir -p $OBJ
for f in imgui imgui_draw imgui_tables imgui_widgets; do
    o=$OBJ/$f.o
    [ -f $o ] && [ $o -nt $IMGUI/$f.cpp ] && [ $o -nt "$HSML/sdk/hsml_imconfig.h" ] || g++ -O2 -std=c++17 $IMFLAGS -c $IMGUI/$f.cpp -o $o
done
g++ $FLAGS $IMFLAGS -DHOOKSHOT -DRD_MENU -I. -I../src -I"$HSML/sdk" $LINK -o $DLL ../src/main.cpp hs_rt.cpp menu_preview.cpp ui_paint.cpp "$HSML/sdk/hsml_mod.cpp" "$HSML/sdk/hsml_menu.cpp" $OBJ/*.o
# what HSML's mod list shows (mods\Ragdoll Button\mod.txt), with this build's version
sed "s/@VERSION@/$(sed -n 's/.*MOD_VERSION = "\([^"]*\)".*/\1/p' ../src/config.inc)/" mod.txt.in > ../out/hookshot/mod.txt
echo built $DLL
