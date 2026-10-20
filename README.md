# OpenEnroth for PS5

A native PlayStation 5 homebrew port of [OpenEnroth](https://github.com/OpenEnroth/OpenEnroth), the open-source
reimplementation of the Might and Magic VI–VIII engine. It runs **Might and Magic VII** as a home-screen title on
a jailbroken PS5: native x86-64 code, OpenGL 4.6 on the console's GPU, DualSense input and console audio. No
emulation or Wine. Might and Magic VI support is the next goal.

> **Status: early.** It boots into the game and plays, but controller support is incomplete: the sticks don't
> work yet and there is no cursor for the inventory, spellbook, shops or dialogue.

Bring your own game files: nothing from Might and Magic is included.

## Installing

See [docs/INSTALL.md](docs/INSTALL.md). In short: a PS5 on firmware 13.60 or lower with kstuff and ShadowMountPlus,
your own Might and Magic VII (GOG or Steam), and `python3 install.py --ps5 <ip> --mm7 <game folder>` from a release.

## Building

Builds on x86-64 Linux. You need:

- **LLVM 21** clang, lld and llvm (the toolchain the PS5 OpenGL driver was validated with).
  Arch: `pacman -S clang21 lld21 llvm21`. Debian/Ubuntu: `clang-21 lld-21 llvm-21` from apt.llvm.org, and set
  `LLVM21=/usr/lib/llvm-21` in `config.local.sh`.
- git, curl, wget, unzip, make, cmake (3.27+), ninja, python3 with venv, gcc, bison, flex,
  glslangValidator, spirv-tools.
- About 15 GB of disk space.

```sh
cp config.example.sh config.local.sh   # set PS5_HOST and MM7_DATA
scripts/setup.sh                       # fetch, verify and build the dependencies into deps/ (~10 minutes)
scripts/build.sh                       # build OpenEnroth and package build/title/dist/PPSA99967
scripts/package.sh deploy-data         # once: upload your MM7 files to the console
scripts/build.sh deploy                # build and upload the program
scripts/build.sh release               # build a release zip in build/release
```

### Working on OpenEnroth

`scripts/setup.sh` checks out OpenEnroth at a pinned commit in `deps/openenroth` and applies
`patches/openenroth` on a `ps5` branch. Make changes there, commit them to that branch, then run
`scripts/export-patches.sh` and commit the updated patches here. A desktop build of the same tree
(`-DOE_USE_SDL2=ON -DOE_GL33=ON`) is a quick way to test changes before going to the console.

### Debugging on the console

Native titles have no console output, so stdout and stderr go to the kernel log:

```sh
scripts/send-payload.sh klogsrv   # once per console restart: kernel log server on port 3232
scripts/klog.sh 600               # record 10 minutes of kernel log into logs/, then launch the title
```

A crash shows up there as "A user thread receives a fatal signal" with a backtrace. The title is loaded at
`0x400000`; subtract that and look the addresses up with
`$LLVM21/bin/llvm-symbolizer --obj=build/title/build/llvm-pie.elf <address>`.

`tools/smoke-test` builds a small title (PPSA99961) that checks the console side on its own: OpenGL context and
shader compilation, display mode, and every DualSense input, rumble and light bar.

## How it works

- **OpenEnroth** gets three patches: an **SDL2 platform backend** (the PS5 homebrew SDL is 2.30, OpenEnroth uses
  SDL3), an **OpenGL 3.3 core** mode (what the driver's SDL2 bridge accepts), and a **`ps5` build platform**.
- **Graphics** come from [ps5-opengl](https://github.com/mihawk-99/ps5-opengl) v1.0.0, a static Mesa/Gallium
  OpenGL 4.6 driver for the console's GPU. Native titles can't load shared libraries, so it is linked in.
- **The title** is built with the PS5 payload SDK and the native app template's runtime, linker conversion and
  signing (`scripts/package.sh`).
- **`platform/`** fills the gaps between those libraries and what the console's libc exports to a native title:
  directory listing, locale functions, console output, `exit`, random numbers, and so on. Several of these come
  from the [OpenRCT2 PS5 port](https://github.com/icepuente/rct2-ps5).
- **LuaJIT** is built for LuaJIT's own PS5 target (interpreter only), with the settings OpenEnroth's scripts need.

## Layout

| | |
| --- | --- |
| `scripts/` | setup, build, package, install, debugging |
| `toolchain/` | CMake toolchain file and compiler wrappers for the PS5 |
| `platform/` | runtime glue linked into the title |
| `patches/` | changes to OpenEnroth and ps5-opengl |
| `tools/smoke-test/` | hardware check title |
| `title/` | home-screen art, turned into the icon and backgrounds by `scripts/make-title-art.py` |
| `docs/` | install guide |
| `deps/`, `build/`, `logs/` | created by the scripts, not committed |

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). Third-party components and their licenses are listed in
[THIRD_PARTY.md](THIRD_PARTY.md).

Might and Magic is a trademark of Ubisoft. This project is not affiliated with or endorsed by Ubisoft, New World
Computing or Sony Interactive Entertainment.
