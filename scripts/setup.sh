#!/usr/bin/env bash
# Fetches, verifies and builds everything the PS5 build needs into deps/. Safe to re-run: finished steps are
# skipped (deps/.stamps), so after changing a pin, delete its stamp or the deps/ entry to redo that step.
#
# Every source is pinned to an exact commit or checksum. Some upstreams were deleted in 2026 and only survive
# as mirrors; override the URLs below from the environment if a mirror disappears.
set -euo pipefail

. "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"

# --- Pins -------------------------------------------------------------------------------------------------
# Native app template (BlackBearReloaded, upstream deleted). 4f531c4 is the commit ps5-opengl v1.0.0 pins; it
# carries the RELRO writer fix without which large titles fail to launch with CE-107750-0. Its own scripts fetch
# and verify the payload SDK (v0.42) and the PacBrew ports (v0.40.2).
TEMPLATE_URL=${TEMPLATE_URL:-https://github.com/efkann/ps5-native-app-boilerplate.git}
TEMPLATE_COMMIT=4f531c4b517f80bcb6b1267135848168d2250047
# ps5-opengl (BlackBearReloaded, upstream deleted): static OpenGL 4.6 for native titles, and its SDL2 bridge.
PS5_OPENGL_URL=${PS5_OPENGL_URL:-https://github.com/mihawk-99/ps5-opengl.git}
PS5_OPENGL_TAG=v1.0.0
PS5_OPENGL_COMMIT=dce74910af7f4274c48d05e0ae001b3038462588
# The PS5 SDL2 fork ps5-opengl's SDL2 bridge is built against. patches/ps5-sdl go on top of ps5-opengl's own SDL patches.
PS5_SDL_URL=${PS5_SDL_URL:-https://github.com/ps5-payload-dev/SDL.git}
PS5_SDL_COMMIT=8c56053f13ca13a0c050de613706ff69eb615836
# OpenEnroth, with patches/openenroth applied on a ps5 branch.
OPENENROTH_URL=${OPENENROTH_URL:-https://github.com/OpenEnroth/OpenEnroth.git}
OPENENROTH_COMMIT=e355c999ca1351a8ee58ad094a70ffa2312bc16b
# Python tools for building Mesa, and for the title art (scripts/make-title-art.py).
PYTHON_PACKAGES="meson==1.12.1 mako==1.4.3 markupsafe==3.0.4 pyyaml==6.0.3 packaging==26.3 pillow==12.3.0 etcpak==0.9.15"
# ----------------------------------------------------------------------------------------------------------

stamps=$MM_PS5_DEPS/.stamps
mkdir -p "$MM_PS5_DEPS" "$stamps"
jobs=$(nproc)

step_done() { [[ -f $stamps/$1 && $(cat "$stamps/$1") == "$2" ]]; }
mark_done() { printf '%s' "$2" > "$stamps/$1"; }
say() { printf '\n==> %s\n' "$*"; }

# Clones $url into $dir at exactly $commit (fetching the commit by hash, which mirrors of deleted repos need).
checkout_commit() {
    local url=$1 dir=$2 commit=$3
    if [[ ! -d $dir/.git ]]; then
        rm -rf "$dir"
        git init -q "$dir"
        git -C "$dir" remote add origin "$url"
    fi
    git -C "$dir" fetch -q --depth 1 origin "$commit"
    git -C "$dir" checkout -q --force "$commit"
    [[ $(git -C "$dir" rev-parse HEAD) == "$commit" ]] || { echo "$dir is not at $commit" >&2; exit 1; }
}

say "Checking host tools"
missing=()
for tool in git curl wget unzip make cmake ninja python3 gcc bison flex glslangValidator spirv-val tar sha256sum; do
    command -v "$tool" >/dev/null || missing+=("$tool")
done
for tool in clang clang++ ld.lld llvm-config llvm-ar llvm-nm llvm-readelf; do
    [[ -x $LLVM21/bin/$tool ]] || missing+=("$LLVM21/bin/$tool")
done
[[ ${#missing[@]} -eq 0 ]] || { printf 'Missing: %s\nSee README.md, "Building".\n' "${missing[*]}" >&2; exit 2; }
"$LLVM21/bin/clang" --version | head -1
[[ $("$LLVM_CONFIG" --version) == 21.* ]] || echo "WARNING: $LLVM_CONFIG is not LLVM 21; this was only validated with 21.1.8" >&2

if ! step_done venv "$PYTHON_PACKAGES"; then
    say "Python tools ($PYTHON_PACKAGES)"
    rm -rf "$MM_PS5_DEPS/venv"
    python3 -m venv "$MM_PS5_DEPS/venv"
    # shellcheck disable=SC2086
    "$MM_PS5_DEPS/venv/bin/pip" install -q $PYTHON_PACKAGES
    mark_done venv "$PYTHON_PACKAGES"
fi

say "clang-18 names for LLVM 21"
mkdir -p "$MM_PS5_DEPS/toolshim"
ln -sf "$LLVM21/bin/clang" "$MM_PS5_DEPS/toolshim/clang-18"
ln -sf "$LLVM21/bin/clang++" "$MM_PS5_DEPS/toolshim/clang++-18"

template=$PS5_NATIVE_APP_TEMPLATE
if ! step_done template "$TEMPLATE_COMMIT"; then
    say "Native app template $TEMPLATE_COMMIT, payload SDK, PacBrew ports"
    checkout_commit "$TEMPLATE_URL" "$template" "$TEMPLATE_COMMIT"
    chmod +x "$template/tooling/prospero-clang18" # CMake runs it directly; the template only runs it through sh.
    bash "$template/tools/setup-native-dependencies.sh"
    make -C "$template" --no-print-directory libc
    bash "$template/tools/setup-pacbrew-dependencies.sh" --list > /dev/null
    mark_done template "$TEMPLATE_COMMIT"
fi

gl=$MM_PS5_DEPS/ps5-opengl
gl_pin="$PS5_OPENGL_COMMIT $(cat "$MM_PS5_ROOT"/patches/ps5-opengl/*.patch | sha256sum | cut -c1-16)"
if ! step_done ps5-opengl "$gl_pin"; then
    say "ps5-opengl $PS5_OPENGL_TAG: sources, Mesa, OpenGL 4.6 SDK (several minutes)"
    checkout_commit "$PS5_OPENGL_URL" "$gl" "$PS5_OPENGL_COMMIT"
    for patch in "$MM_PS5_ROOT"/patches/ps5-opengl/*.patch; do
        git -C "$gl" apply "$patch"
    done
    (cd "$gl" && python3 tools/fetch-sources.py)
    # Meson build directories remember absolute paths (SDK cross file), so never reuse an old one. The SDL2
    # build lives in there too, so its step has to run again.
    rm -rf "$gl/build" "$stamps/sdl2"
    MAKEFLAGS=-j$jobs make -C "$gl" --no-print-directory sdk-gl46 > "$MM_PS5_DEPS/ps5-opengl-build.log" 2>&1 ||
        { tail -30 "$MM_PS5_DEPS/ps5-opengl-build.log"; echo "ps5-opengl build failed, see deps/ps5-opengl-build.log" >&2; exit 1; }
    mark_done ps5-opengl "$gl_pin"
fi

sdl_pin="$PS5_SDL_COMMIT $(cat "$MM_PS5_ROOT"/patches/ps5-sdl/*.patch | sha256sum | cut -c1-16) $gl_pin"
if ! step_done sdl2 "$sdl_pin"; then
    say "SDL2 $PS5_SDL_COMMIT with patches/ps5-sdl, ps5-opengl's video driver and the PS5 audio driver"
    checkout_commit "$PS5_SDL_URL" "$MM_PS5_DEPS/ps5-sdl" "$PS5_SDL_COMMIT"
    rm -rf "$gl/build/sdl2-native"
    # build.py builds an archive of the pinned commit with its own patches, then ours (patches/ps5-opengl/0002).
    sdl_patches=()
    for patch in "$MM_PS5_ROOT"/patches/ps5-sdl/*.patch; do
        sdl_patches+=(--extra-patch "$patch")
    done
    python3 "$gl/integration/SDL2/build.py" native --sdl-source "$MM_PS5_DEPS/ps5-sdl" \
        --sdk-prefix "$gl/build/sdk/ps5-opengl-gl46" --out "$gl/build/sdl2-native" \
        --payload-sdk "$PS5_PAYLOAD_SDK" --compiler-wrapper "$template/tooling/prospero-clang18" "${sdl_patches[@]}" \
        > "$MM_PS5_DEPS/sdl2-build.log" 2>&1 ||
        { tail -30 "$MM_PS5_DEPS/sdl2-build.log"; echo "SDL2 build failed, see deps/sdl2-build.log" >&2; exit 1; }
    mark_done sdl2 "$sdl_pin"
fi

oe=$MM_PS5_DEPS/openenroth
oe_pin="$OPENENROTH_COMMIT $(cat "$MM_PS5_ROOT"/patches/openenroth/*.patch | sha256sum | cut -c1-16)"
if ! step_done openenroth "$oe_pin"; then
    say "OpenEnroth $OPENENROTH_COMMIT with patches/openenroth"
    if [[ -d $oe/.git ]] && [[ -n $(git -C "$oe" status --porcelain --untracked-files=no) ]]; then
        echo "deps/openenroth has uncommitted changes; commit them on its ps5 branch and run scripts/export-patches.sh first" >&2
        exit 1
    fi
    checkout_commit "$OPENENROTH_URL" "$oe" "$OPENENROTH_COMMIT"
    git -C "$oe" submodule update -q --init --recursive --depth 1
    git -C "$oe" checkout -q -B ps5
    git -C "$oe" -c user.name="openenroth-ps5 setup" -c user.email="setup@localhost" am -q "$MM_PS5_ROOT"/patches/openenroth/*.patch
    mark_done openenroth "$oe_pin"
fi

luajit_pin="$OPENENROTH_COMMIT $(sha256sum "$MM_PS5_ROOT/scripts/build-luajit.sh" | cut -c1-16)"
if ! step_done luajit "$luajit_pin"; then
    say "LuaJIT for the PS5 title"
    bash "$MM_PS5_ROOT/scripts/build-luajit.sh" > "$MM_PS5_DEPS/luajit-build.log" 2>&1 ||
        { tail -30 "$MM_PS5_DEPS/luajit-build.log"; echo "LuaJIT build failed, see deps/luajit-build.log" >&2; exit 1; }
    mark_done luajit "$luajit_pin"
fi

say "Dependencies ready. Next: scripts/build.sh"
