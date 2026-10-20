#!/usr/bin/env bash
# Lists imports of a linked native title that only system modules outside the loader's preloaded set
# provide. Such imports link fine but stay null at run time, so the first call jumps to address 0;
# each needs a definition in platform/ (lazily loading the module, or a replacement).
#
# Same approach and preloaded-module list as icepuente/rct2-ps5's scripts/check-imports.sh, whose
# list came from the kernel log of a native title launch on firmware 13.60.
#
# Usage: scripts/check-imports.sh <llvm-pie.elf>
set -euo pipefail

elf=$1
. "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
stubs="$PS5_PAYLOAD_SDK/target/lib"
nm="$LLVM21/bin/llvm-nm"
readelf="$LLVM21/bin/llvm-readelf"

preloaded_modules="libkernel libkernel_sys libkernel_web libSceLibcInternal libSceSysmodule
libSceAmpr libSceNet libSceIpmi libSceMbus libSceRegMgr libSceRtc libSceRazorCpu
libSceAvSetting libSceVideoOut libSceAgcDriver libSceAgc libSceAudioOut
libSceAudioIn libSceAjmi libSceAjm libScePad libSceNetCtl libSceSsl
libSceHttpCache libSceHttp libSceHttp2 libSceNpCommon libSceNpManager
libSceNpGameIntent libSceNpWebApi2 libSceSaveData libSceSystemService
libSceUserService libSceCommonDialog libSceSysUtil"
declare -A preloaded
for module in $preloaded_modules; do preloaded[$module]=1; done

declare -A providers
for so in "$stubs"/*.so; do
    module=$(basename "$so" .so)
    [[ $module == *_stub_weak ]] && continue
    while read -r symbol; do
        providers[$symbol]+="$module "
    done < <("$nm" -D --defined-only "$so" 2>/dev/null | awk '{print $NF}')
done

status=0
while read -r symbol; do
    modules=${providers[$symbol]:-}
    [[ -z $modules ]] && continue
    resolved=0
    for module in $modules; do [[ -n ${preloaded[$module]:-} ]] && resolved=1; done
    if (( ! resolved )); then
        echo "null at run time: $symbol  (only in: ${modules% })"
        status=1
    fi
done < <("$readelf" --dyn-syms "$elf" | awk '$7 == "UND" && $8 != "" {sub(/@.*/, "", $8); print $8}' | sort -u)

(( status )) || echo "All imports resolve to preloaded modules."
exit $status
