#!/usr/bin/env bash
# Regenerates patches/openenroth from the commits on deps/openenroth's ps5 branch. Work on OpenEnroth there:
# commit to the ps5 branch, run this, and commit the updated patches here.
set -euo pipefail

. "$(dirname -- "${BASH_SOURCE[0]}")/env.sh"
oe=$MM_PS5_DEPS/openenroth
base=$(sed -n 's/^OPENENROTH_COMMIT=//p' "$MM_PS5_ROOT/scripts/setup.sh")

[[ -z $(git -C "$oe" status --porcelain --untracked-files=no) ]] || { echo "deps/openenroth has uncommitted changes" >&2; exit 1; }
rm -f "$MM_PS5_ROOT"/patches/openenroth/*.patch
git -C "$oe" format-patch -q --no-signature --no-numbered --zero-commit -o "$MM_PS5_ROOT/patches/openenroth" "$base..ps5"
ls "$MM_PS5_ROOT/patches/openenroth"
