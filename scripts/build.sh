#!/usr/bin/env bash
# Builds OpenEnroth for the PS5 into build/openenroth, then links and packages the title (scripts/package.sh).
# Arguments are passed to package.sh: nothing, deploy, deploy-data or release.
set -euo pipefail

. "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
oe_build=$MM_PS5_BUILD/openenroth

[[ -f $MM_PS5_DEPS/.stamps/luajit ]] || { echo "Run scripts/setup.sh first" >&2; exit 2; }

# OE_PRESENT_WITH_DRAW: the PS5 driver can only blit between matching formats on the GPU, and OpenEnroth's scaled
# blit to the screen fell back to the CPU (about 10 fps). OE_GAMEPAD_TOUCHPAD_MOUSE: the DualSense touchpad is the
# mouse. OE_GAMEPAD_MENU_NAVIGATION: the d-pad moves the cursor between buttons and items in menus.
cmake -S "$MM_PS5_DEPS/openenroth" -B "$oe_build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$MM_PS5_ROOT/toolchain/ps5.cmake" -DCMAKE_BUILD_TYPE=Release \
    -DOE_USE_SDL2=ON -DOE_GL33=ON -DOE_PRESENT_WITH_DRAW=ON -DOE_GAMEPAD_TOUCHPAD_MOUSE=ON -DOE_GAMEPAD_MENU_NAVIGATION=ON \
    -DOE_USE_PREBUILT_DEPENDENCIES=OFF -DOE_BUILD_TESTS=OFF -DOE_BUILD_TOOLS=OFF \
    -DOE_CHECK_STYLE=OFF -DOE_CHECK_LUA_STYLE=OFF -DOE_CHECK_TIDY=OFF \
    -DOE_USE_LD_MOLD=OFF -DOE_USE_LD_LLD=OFF -DOE_USE_CCACHE=OFF -DOE_USE_SCCACHE=OFF > /dev/null
cmake --build "$oe_build" --target OpenEnroth -j"$(nproc)"

exec "$MM_PS5_ROOT/scripts/package.sh" "$@"
