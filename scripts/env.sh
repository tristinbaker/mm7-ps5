# Build environment for the PS5 port. Sourced by the other scripts; source it yourself to run the tools by hand:
#   . scripts/env.sh
#
# Per-machine settings (console address, where your game files are) go in config.local.sh at the repository
# root, see config.example.sh. It is not committed.

MM_PS5_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]:-$0}")/.." && pwd)
export MM_PS5_ROOT
export MM_PS5_DEPS="$MM_PS5_ROOT/deps"
export MM_PS5_BUILD="$MM_PS5_ROOT/build"

# Defaults, overridable in config.local.sh or the environment.
: "${PS5_HOST:=}"
: "${FTP_PORT:=2121}"
: "${ELFLDR_PORT:=9021}"
: "${MM7_DATA:=}"
: "${LLVM21:=/usr/lib/llvm21}"
[[ -f $MM_PS5_ROOT/config.local.sh ]] && . "$MM_PS5_ROOT/config.local.sh"
export PS5_HOST FTP_PORT ELFLDR_PORT MM7_DATA LLVM21

# The payload SDK's wrappers and ps5-opengl v1.0.0 were validated with LLVM 21.1.8 (clang + lld). Several build
# scripts call clang-18 by name; deps/toolshim maps that to the same clang 21.
export LLVM_CONFIG="$LLVM21/bin/llvm-config"
export PS5_CLANG="$LLVM21/bin/clang"
export PATH="$MM_PS5_DEPS/venv/bin:$MM_PS5_DEPS/toolshim:$PATH"

export PS5_NATIVE_APP_TEMPLATE="$MM_PS5_DEPS/ps5-native-app-boilerplate"
export PS5_PAYLOAD_SDK="$PS5_NATIVE_APP_TEMPLATE/.deps/native/ps5-payload-sdk"
# The template build uses ccache unless told otherwise.
export USE_CCACHE=0
