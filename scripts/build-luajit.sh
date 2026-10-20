#!/usr/bin/env bash
# Builds LuaJIT for the PS5 title from OpenEnroth's own submodule, into deps/luajit/src/libluajit.a
# (picked up by toolchain/ps5.cmake). Interpreter only: LuaJIT's PS5 target (__PROSPERO__) disables the JIT, and
# LUAJIT_DISABLE_JIT/_PROFILE keep the host-built generators in agreement with it. Lua 5.2 compatibility matches OpenEnroth's
# bundled build; its scripts rely on __pairs. LUAJIT_USE_SYSMALLOC: LuaJIT's own allocator probes mmap at
# hinted addresses, which a native title can't do (luaL_newstate returns NULL); malloc is the title's heap.
set -euo pipefail
. "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
bin=$MM_PS5_ROOT/toolchain/bin
out=$MM_PS5_DEPS/luajit
rm -rf "$out"
cp -a "$MM_PS5_DEPS/openenroth/thirdparty/luajit" "$out"
rm -rf "$out/.git"
make -j"$(nproc)" -C "$out/src" libluajit.a BUILDMODE=static HOST_CC=gcc \
    STATIC_CC="$bin/ps5-cc" DYNAMIC_CC="$bin/ps5-cc" TARGET_LD="$bin/ps5-cc" \
    TARGET_AR="$LLVM21/bin/llvm-ar rcus" TARGET_STRIP=true \
    XCFLAGS="-DLUAJIT_ENABLE_LUA52COMPAT -DLUAJIT_DISABLE_JIT -DLUAJIT_DISABLE_PROFILE -DLUAJIT_USE_SYSMALLOC" \
    TARGET_CFLAGS="-D__PROSPERO__ -fPIC -ffunction-sections -fdata-sections -funwind-tables" Q=
ls -la "$out/src/libluajit.a"
