# Third-party components

The PS5 title is built from the components below. Except for the files in `platform/` that say so, none of
them are copied into this repository: `scripts/setup.sh` fetches each one at the pinned version and checks it.
Because several of them are GPL-3.0-or-later, the title as a whole is distributed under GPL-3.0-or-later (see
`LICENSE`); the source for everything in it is available from the projects below at the listed versions, plus
this repository.

No Might and Magic game data is included anywhere. You need your own copy of the game. The one exception is the
home-screen art, `title/art.png`: the Might and Magic VII cover art, which belongs to Ubisoft and is not covered
by this repository's license.

| Component | Version | License | Used for |
| --- | --- | --- | --- |
| [OpenEnroth](https://github.com/OpenEnroth/OpenEnroth) | `e355c99` + `patches/openenroth` | LGPL-3.0 | The game engine. Its bundled libraries (fmt, spdlog, Dear ImGui, glm, sol2, nlohmann/json, glad, ...) keep their own permissive licenses. |
| [ps5-opengl](https://github.com/mihawk-99/ps5-opengl) by BlackBearReloaded (original repository deleted; mirror) | v1.0.0 (`dce7491`) + `patches/ps5-opengl` | GPL-3.0-or-later; includes Mesa 26.2.0 (MIT and others) and OpenGNM-PSBC | Static OpenGL 4.6 driver and its SDL2 video driver |
| [ps5-native-app-boilerplate](https://github.com/efkann/ps5-native-app-boilerplate) by BlackBearReloaded (original repository deleted; mirror) | `4f531c4` | GPL-3.0-or-later | Native title runtime (`libc.prx` loader shim, CRT), linker conversion and FSELF signing |
| [ps5-payload-sdk](https://github.com/ps5-payload-dev/sdk) | v0.42 | GPL-3.0-or-later (FreeBSD headers BSD) | Toolchain wrappers, libc++/libunwind, system library stubs |
| [PacBrew ports](https://github.com/ps5-payload-dev/pacbrew-repo) | v0.40.2 | per package: FFmpeg 7.0 (built `--enable-gpl --enable-version3`: GPL-3.0), x264 (GPL-2.0-or-later), OpenAL Soft (LGPL-2.0-or-later), libpng (libpng), zlib (zlib), OpenSSL (Apache-2.0), xz (0BSD), bzip2 (bzip2), libiconv (LGPL) | Audio, video decoding, image and compression libraries |
| [ps5-payload-dev/SDL](https://github.com/ps5-payload-dev/SDL) | `8c56053` (SDL 2.30.12) | zlib | Window, input, audio |
| [LuaJIT](https://luajit.org) | OpenEnroth's submodule | MIT | Scripting (built by `scripts/build-luajit.sh`) |
| [rct2-ps5](https://github.com/icepuente/rct2-ps5) | `115c231` | GPL-3.0-or-later | `platform/ps5_dirent.c`, `ps5_libc_compat.c`, `ps5_thread_atexit.c` (copies), and parts of `ps5_console.c` and `ps5_lazy_modules.c` (adapted); originals in `platform/upstream-rct2-ps5` |
| [stb_easy_font](https://github.com/nothings/stb) | `a7d4c21` | MIT or public domain | Text in the smoke test (`tools/smoke-test`) |

`scripts/send-payload.sh klogsrv` downloads [klogsrv](https://github.com/ps5-payload-dev/klogsrv) v0.9 for
debugging; it isn't part of the title.

Might and Magic is a trademark of Ubisoft. This project is not affiliated with or endorsed by Ubisoft, New World
Computing or Sony Interactive Entertainment.
