#!/usr/bin/env bash
# Build the MM-PS5 smoke test into a native folder title.
#
# This follows ps5-opengl's integration/SDL2/folder.py: a copy of the native app
# template, ps5-opengl's heap and runtime shims, malloc wrapped onto that heap,
# and one static link group of SDL2 + our object + the GL SDK + libc++.
# It differs only in linking our own object and using our own title metadata.
#
# A hardware check for the console side of the port: draws with OpenGL, shows the GL version, display mode and
# live DualSense state on screen, and tests rumble (R3) and the light bar (L3).
#
# Usage: tools/smoke-test/build.sh            build build/smoke-test/dist/PPSA99961
#        tools/smoke-test/build.sh deploy     build, then upload to $PS5_HOST over FTP
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$here/../../scripts/env.sh"
gl=$MM_PS5_DEPS/ps5-opengl
gl_sdk=$gl/build/sdk/ps5-opengl-gl46
template=$PS5_NATIVE_APP_TEMPLATE
sdl_native=$gl/build/sdl2-native
out=$MM_PS5_BUILD/smoke-test
title_id=PPSA99961
title_name="MM-PS5 Smoke Test"

[[ -f $sdl_native/sdk/lib/libSDL2.a ]] || { echo "Dependencies not built, run scripts/setup.sh" >&2; exit 2; }

# 1. A private copy of the template, since the SDK copy gets AGC link stubs added.
rm -rf "$out"
mkdir -p "$out/src" "$out/vendor" "$out/build/native-imports"
for directory in runtime sce_sys tooling tools .deps/native; do
    mkdir -p "$(dirname "$out/$directory")"
    cp -a "$template/$directory" "$out/$directory"
done
sdk=$out/.deps/native/ps5-payload-sdk
cp "$gl/native-app/runtime_shims.c" "$gl/native-app/app_heap.c" "$MM_PS5_ROOT/platform/fortify_compat.c" "$out/src/"
cp "$out/tooling/native/ps5-pie.ld" "$out/tooling/native/ps5-pie-base.ld"
cp "$gl/native-app/ps5-pie.ld" "$gl/native-app/app-symbols.map" "$out/tooling/native/"
cp "$gl_sdk/lib/libSceAgc.so" "$gl_sdk/lib/libSceAgcDriver.so" "$sdk/target/lib/"

python3 - "$gl/native-app/param.json" "$out/sce_sys/param.json" "$title_id" "$title_name" <<'PY'
import json, sys
source, target, title_id, name = sys.argv[1:]
param = json.load(open(source))
digits = title_id[4:]
param["titleId"] = title_id
param["conceptId"] = digits
param["contentId"] = f"UP9000-{title_id}_00-MMPS5SMOKETEST01"
language = param["localizedParameters"]["defaultLanguage"]
param["localizedParameters"][language]["titleName"] = name
open(target, "w").write(json.dumps(param, indent=2) + "\n")
PY

python3 - "$out" <<'PY'
import sys
from pathlib import Path
out = Path(sys.argv[1])
def replace_once(path, old, new):
    text = path.read_text()
    if text.count(old) != 1:
        raise SystemExit(f"template changed, cannot patch {path}: {old}")
    path.write_text(text.replace(old, new))
replace_once(out / "tooling/native/sce_module_writer.cpp",
             "write_u64(result.data, result.heap_size, std::numeric_limits<std::uint64_t>::max());",
             "write_u64(result.data, result.heap_size, 0x10000000ULL);")
script = out / "tools/build.sh"
replace_once(script, 'bash "$root/tools/setup-native-dependencies.sh" >/dev/null',
             'test -x "$root/.deps/native/ps5-payload-sdk/bin/prospero-lld"')
replace_once(script, '[[ -f $root/runtime/libc.prx ]] || bash "$root/tools/rebuild-libc.sh"',
             'test -f "$root/runtime/libc.prx"')
replace_once(script, "--eh-frame-hdr \\",
             "--eh-frame-hdr --wrap=malloc --wrap=calloc --wrap=realloc --wrap=free "
             "--wrap=posix_memalign --wrap=malloc_usable_size \\")
# This script builds the folder itself, so deploy only uploads it.
replace_once(out / "tools/deploy.sh", 'make -C "$root" --no-print-directory "$build_target"',
             ': # already built by tools/smoke-test/build.sh')
PY

# 2. Our object, compiled the way the SDL2 example is.
PS5_PAYLOAD_SDK="$sdk" sh "$template/tooling/prospero-clang18" \
    -std=c11 -O2 -Wall -Wextra -D__PROSPERO__ -DSDL_MAIN_HANDLED=1 -fPIC -ffunction-sections -fdata-sections \
    -I"$sdl_native/sdk/include/SDL2" -I"$gl_sdk/include" -I"$here/third_party" \
    -c "$here/smoke.c" -o "$out/vendor/smoke.o"

# 3. One link group, then the template's own sign-and-assemble step.
compiler_rt=$LLVM21/lib/clang/21/lib/linux/libclang_rt.builtins-x86_64.a
libraries=("$sdl_native/sdk/lib/libSDL2.a" "$out/vendor/smoke.o" "$gl_sdk/lib/libPS5OpenGLCore33.a"
           "$sdk/target/lib/libunwind.a" "$sdk/target/lib/libc++abi.a" "$sdk/target/lib/libc++.a" "$compiler_rt")
for library in "${libraries[@]}"; do
    [[ -f $library ]] || { echo "missing link input: $library" >&2; exit 2; }
done
{
    printf 'SEARCH_DIR("%s/target/lib")\nSEARCH_DIR("%s/lib")\nEXTERN(ps5_agc_gate2_run)\nGROUP (\n' "$sdk" "$gl_sdk"
    printf '  "%s"\n' "${libraries[@]}"
    printf ')\n'
} > "$out/vendor/libmmps5.a"

(
    cd "$out"
    export PS5_PAYLOAD_SDK="$sdk" APP_STATIC_ARCHIVES=vendor/libmmps5.a
    export APP_DEFINITIONS= APP_INCLUDE_PATHS= PACBREW_PACKAGES= PACBREW_INCLUDE_PATHS= PACBREW_STATIC_ARCHIVES= APP_RUNTIME_MODULES=
    bash tools/build.sh Folder
)

# The PS5 loader refuses a title whose PT_LOAD file offset and address differ modulo the
# 16 KiB page size (CE-107750-0); older template writers produced that for RELRO.
"$LLVM21/bin/llvm-readelf" -lW "$out/build/eboot.elf" | awk '
    $1 == "LOAD" { if (strtonum($2) % 16384 != strtonum($3) % 16384) { print "incongruent PT_LOAD: " $0; bad = 1 } }
    END { exit bad }'
echo "Built $out/dist/$title_id"

if [[ ${1:-} == deploy ]]; then
    (cd "$out" && bash tools/deploy.sh)
fi
