# CMake toolchain for PS5 native-title code (jailbroken console, ShadowMountPlus folder titles).
#
# Compiles with the native app template's target wrapper (x86_64-sie-ps5, the payload SDK's libc and
# libc++) driven by LLVM 21.1.8, the toolchain ps5-opengl v1.0.0 was validated with. CMake only builds
# static libraries and objects here: executables are "linked" by ps5-capture-link, which records their
# inputs so that scripts/package.sh can link, convert and sign them with the template's own tooling.
#
# Dependencies come from three prefixes, searched in this order:
#   1. ps5-opengl's SDL2 build (SDL 2.30.12 with the G19 OpenGL video driver and the PS5 audio driver)
#   2. the ps5-opengl OpenGL 4.6 SDK
#   3. the template's pinned PacBrew sysroot (OpenAL, FFmpeg, libpng, zlib)
# The PacBrew sysroot also ships an SDL2 without the G19 driver, so the SDL2 build must come first.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

get_filename_component(MM_PS5_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(MM_PS5_DEPS "${MM_PS5_ROOT}/deps")
if(DEFINED ENV{LLVM21})
    set(LLVM21 "$ENV{LLVM21}")
else()
    set(LLVM21 /usr/lib/llvm21)
endif()
set(PS5_NATIVE_APP_TEMPLATE "${MM_PS5_DEPS}/ps5-native-app-boilerplate")
set(PS5_PAYLOAD_SDK "${PS5_NATIVE_APP_TEMPLATE}/.deps/native/ps5-payload-sdk")
set(PS5_SDL2_PREFIX "${MM_PS5_DEPS}/ps5-opengl/build/sdl2-native/sdk")
set(PS5_OPENGL_PREFIX "${MM_PS5_DEPS}/ps5-opengl/build/sdk/ps5-opengl-gl46")
set(PS5_PACBREW_PREFIX "${PS5_NATIVE_APP_TEMPLATE}/.deps/pacbrew/v0.40.2/sysroot/user/homebrew")

set(CMAKE_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/bin/ps5-cc")
set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/bin/ps5-c++")
set(CMAKE_AR "${LLVM21}/bin/llvm-ar")
set(CMAKE_RANLIB "${LLVM21}/bin/llvm-ranlib")

# Same code generation flags as ps5-opengl's SDL2 build. The wrapper adds the target, sysroot,
# -fno-stack-protector, -fno-plt and -femulated-tls.
# The x86_64-sie-ps5 target turns C++ exceptions and RTTI off by default. The payload SDK's libc++ is built
# with both, and OpenEnroth needs both. C code gets unwind tables so exceptions can pass through C frames
# (LuaJIT and SDL callbacks).
set(CMAKE_C_FLAGS_INIT "-D__PROSPERO__ -fPIC -ffunction-sections -fdata-sections -funwind-tables")
set(CMAKE_CXX_FLAGS_INIT "-D__PROSPERO__ -fPIC -ffunction-sections -fdata-sections -fexceptions -fcxx-exceptions -frtti")

set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/bin/ps5-capture-link <TARGET> <OBJECTS> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CURRENT_LIST_DIR}/bin/ps5-capture-link <TARGET> <OBJECTS> <LINK_LIBRARIES>")

set(CMAKE_FIND_ROOT_PATH "${PS5_SDL2_PREFIX}" "${PS5_OPENGL_PREFIX}" "${PS5_PACBREW_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# The prefixes are installed at their own root, so search them as "/".
set(CMAKE_SYSTEM_PREFIX_PATH "/")
set(SDL2_DIR "${PS5_SDL2_PREFIX}/lib/cmake/SDL2")
# LuaJIT built by scripts/build-luajit.sh: interpreter only (LuaJIT's own PS5 target), with Lua 5.2 compatibility
# like OpenEnroth's bundled build. PacBrew's LuaJIT lacks the latter.
set(LUAJIT_INCLUDE_DIR "${MM_PS5_DEPS}/luajit/src" CACHE PATH "")
set(LUAJIT_LIBRARY "${MM_PS5_DEPS}/luajit/src/libluajit.a" CACHE FILEPATH "")
set(PS5OpenGL_DIR "${PS5_OPENGL_PREFIX}/lib/cmake/PS5OpenGL")

set(CMAKE_PROJECT_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/project-include.cmake")

set(PS5 ON)
