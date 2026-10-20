#!/usr/bin/env bash
# Links the PS5 build of OpenEnroth into a native folder title, and deploys it or makes a release zip.
#
# Takes the link inputs that CMake recorded for the OpenEnroth executable (toolchain/bin/ps5-capture-link) and
# links them the way ps5-opengl's integration/SDL2/folder.py links an SDL2 + OpenGL title: a private copy of the
# native app template, malloc wrapped onto the GL runtime's heap, and one static link group with libc++.
# The game data is never part of the title; scripts/install.py uploads it from the user's own copy.
#
# Usage: scripts/package.sh                build build/title/dist/PPSA99967
#        scripts/package.sh deploy         build, then upload the program to $PS5_HOST
#        scripts/package.sh deploy-data    upload the MM7 files from $MM7_DATA (needed once)
#        scripts/package.sh release        build, then make build/release/mm7-ps5-<version>.zip
set -euo pipefail

. "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
gl=$MM_PS5_DEPS/ps5-opengl
gl_sdk=$gl/build/sdk/ps5-opengl-gl46
template=$PS5_NATIVE_APP_TEMPLATE
oe_build=$MM_PS5_BUILD/openenroth
link_inputs=$oe_build/src/Bin/OpenEnroth/OpenEnroth.link-inputs
out=$MM_PS5_BUILD/title
title_id=PPSA99967
title_name="Might and Magic VII: For Blood and Honor"
content_id="UP9000-${title_id}_00-OPENENROTHMM7001"
action=${1:-}

if [[ $action == deploy-data ]]; then
    [[ -n $PS5_HOST && -n $MM7_DATA ]] || { echo "Set PS5_HOST and MM7_DATA in config.local.sh" >&2; exit 2; }
    exec python3 "$MM_PS5_ROOT/scripts/install.py" --ps5 "$PS5_HOST" --port "$FTP_PORT" --mm7 "$MM7_DATA" --data-only
fi

[[ -f $link_inputs ]] || { echo "No link inputs; build OpenEnroth first (scripts/build.sh): $link_inputs" >&2; exit 2; }
[[ -f $gl_sdk/lib/libPS5OpenGLCore33.a ]] || { echo "GL SDK not built (scripts/setup.sh): $gl_sdk" >&2; exit 2; }

# 1. A private copy of the template, since the SDK copy gets AGC link stubs added.
rm -rf "$out"
mkdir -p "$out/src" "$out/vendor" "$out/build/native-imports"
for directory in runtime sce_sys tooling tools .deps/native; do
    mkdir -p "$(dirname "$out/$directory")"
    cp -a "$template/$directory" "$out/$directory"
done
sdk=$out/.deps/native/ps5-payload-sdk
cp "$gl/native-app/app_heap.c" "$MM_PS5_ROOT/platform/"*.c "$out/src/"
cp "$out/tooling/native/ps5-pie.ld" "$out/tooling/native/ps5-pie-base.ld"
cp "$gl/native-app/ps5-pie.ld" "$gl/native-app/app-symbols.map" "$out/tooling/native/"
cp "$gl_sdk/lib/libSceAgc.so" "$gl_sdk/lib/libSceAgcDriver.so" "$sdk/target/lib/"
# Home-screen art from title/art.png, replacing the template's placeholders (and its sample selection sound).
"$MM_PS5_DEPS/venv/bin/python" "$MM_PS5_ROOT/scripts/make-title-art.py" "$MM_PS5_ROOT/title/art.png" "$out/sce_sys"
rm -f "$out/sce_sys/snd0.at9"

python3 - "$gl/native-app/param.json" "$out/sce_sys/param.json" "$title_id" "$title_name" "$content_id" <<'PY'
import json, sys
source, target, title_id, name, content_id = sys.argv[1:]
param = json.load(open(source))
param["titleId"] = title_id
param["conceptId"] = title_id[4:]
param["contentId"] = content_id
param["downloadDataSize"] = 1024  # Config, saves and logs in /download0, as much as rct2-ps5 reserves.
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
             "--wrap=posix_memalign --wrap=malloc_usable_size --wrap=exit "
             # Console output to the kernel log and the sysctl fix-up, see platform/ps5_console.c.
             "--wrap=vfprintf --wrap=fputs --wrap=fputc --wrap=fwrite --wrap=fflush --wrap=sysctl "
             "--error-limit=0 \\")
PY

# 2. One link group: OpenEnroth's objects and archives as CMake recorded them, then the C++ runtime.
#    System modules (-l flags, .so stubs) are dropped: the template links every SDK stub as needed.
compiler_rt=$LLVM21/lib/clang/21/lib/linux/libclang_rt.builtins-x86_64.a
libraries=()
while IFS= read -r input; do
    case $input in
        *.o|*.obj|*.a)
            [[ $input == /* ]] || input=$oe_build/$input
            libraries+=("$input") ;;
        -l*|-pthread|*.so) ;;
        *) echo "unexpected link input: $input" >&2; exit 2 ;;
    esac
done < "$link_inputs"
# PacBrew's FFmpeg is configured with x264, OpenSSL and friends, and its codec/protocol tables reference them
# even though OpenEnroth only decodes Bink, Smacker and MP3. Its pkg-config files don't list them.
pacbrew_lib=$template/.deps/pacbrew/v0.40.2/sysroot/user/homebrew/lib
for name in x264 lzma bz2 ssl crypto iconv charset; do
    libraries+=("$pacbrew_lib/lib$name.a")
done
libraries+=("$sdk/target/lib/libunwind.a" "$sdk/target/lib/libc++abi.a" "$sdk/target/lib/libc++.a" "$compiler_rt")
for library in "${libraries[@]}"; do
    [[ -f $library ]] || { echo "missing link input: $library" >&2; exit 2; }
done
{
    printf 'SEARCH_DIR("%s/target/lib")\nSEARCH_DIR("%s/lib")\nEXTERN(ps5_agc_gate2_run)\nGROUP (\n' "$sdk" "$gl_sdk"
    printf '  "%s"\n' "${libraries[@]}"
    printf ')\n'
} > "$out/vendor/libopenenroth.a"

# The startup report of null imports (scripts/make-import-report.py) needs the imports of the linked title, so
# it's made from the previous link's list, and the title is linked again if the list changed.
import_list=$MM_PS5_BUILD/imports.txt
[[ -f $import_list ]] || : > "$import_list"
link_title() {
    python3 "$MM_PS5_ROOT/scripts/make-import-report.py" "$import_list" "$out/src/ps5_import_report.c" > /dev/null
    (
        cd "$out"
        export PS5_PAYLOAD_SDK="$sdk" APP_STATIC_ARCHIVES=vendor/libopenenroth.a
        export APP_DEFINITIONS= APP_INCLUDE_PATHS= PACBREW_PACKAGES= PACBREW_INCLUDE_PATHS= PACBREW_STATIC_ARCHIVES= APP_RUNTIME_MODULES=
        bash tools/build.sh Folder
    )
}
link_title
"$LLVM21/bin/llvm-readelf" --dyn-syms "$out/build/llvm-pie.elf" |
    awk '$7 == "UND" && $8 != "" {sub(/@.*/, "", $8); print $8}' | sort -u > "$import_list.new"
if ! cmp -s "$import_list" "$import_list.new"; then
    mv "$import_list.new" "$import_list"
    echo "Imports changed, linking again for the startup import report"
    link_title
else
    rm -f "$import_list.new"
fi


# 3. Checks the console would otherwise fail on: PT_LOAD congruence (CE-107750-0) and null imports.
"$LLVM21/bin/llvm-readelf" -lW "$out/build/eboot.elf" | awk '
    $1 == "LOAD" { if (strtonum($2) % 16384 != strtonum($3) % 16384) { print "incongruent PT_LOAD: " $0; bad = 1 } }
    END { exit bad }'
"$MM_PS5_ROOT/scripts/check-imports.sh" "$out/build/llvm-pie.elf" || echo "WARNING: imports above will be null at run time"
echo "Built $out/dist/$title_id"

case $action in
    deploy)
        [[ -n $PS5_HOST ]] || { echo "Set PS5_HOST in config.local.sh" >&2; exit 2; }
        python3 "$MM_PS5_ROOT/scripts/install.py" --ps5 "$PS5_HOST" --port "$FTP_PORT" --title "$out/dist/$title_id" --program-only ;;
    release)
        version=$(git -C "$MM_PS5_ROOT" describe --tags --always --dirty)
        staging=$MM_PS5_BUILD/release/mm7-ps5-$version
        rm -rf "$staging" && mkdir -p "$staging"
        cp -a "$out/dist/$title_id" "$staging/"
        cp "$MM_PS5_ROOT/scripts/install.py" "$MM_PS5_ROOT/docs/INSTALL.md" "$MM_PS5_ROOT/LICENSE" "$MM_PS5_ROOT/THIRD_PARTY.md" "$staging/"
        # The console only starts an app whose files are open to everyone, and unzip tools that keep modes would
        # otherwise leave 644 files, so store the title's files as 777.
        python3 - "$staging" "$MM_PS5_BUILD/release/mm7-ps5-$version.zip" "$title_id" <<'PY'
import sys, zipfile
from pathlib import Path
staging, target, title_id = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(staging.rglob("*")):
        name = f"{staging.name}/{path.relative_to(staging).as_posix()}"
        mode = 0o777 if path.relative_to(staging).parts[0] == title_id else (0o755 if path.suffix == ".py" else 0o644)
        info = zipfile.ZipInfo.from_file(path, name)
        info.external_attr = ((0o040000 if path.is_dir() else 0o100000) | mode) << 16
        if path.is_dir():
            archive.writestr(info, "")
        else:
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info, path.read_bytes())
PY
        echo "Release: $MM_PS5_BUILD/release/mm7-ps5-$version.zip" ;;
    "") ;;
    *) echo "unknown action: $action" >&2; exit 2 ;;
esac
