// Functions from system modules that the loader doesn't preload into a native title. Imports from such
// modules link fine but stay null, so these definitions load the module on first use instead.
//
// Module loading, LAZY_IMPORT and arc4random are adapted from icepuente/rct2-ps5 platform/ps5_native_shims.c
// at commit 115c231c9f (GPL-3.0-or-later); getentropy is added for this title.
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int sceKernelLoadStartModule(const char *, size_t, const void *, unsigned int, void *, int *);
int sceKernelDlsym(int, const char *, void **);
const char *sceKernelGetFsSandboxRandomWord(void);
unsigned long long sceKernelGetProcessTime(void);

// Load a module from the title's view of /system/common/lib.
static int loadModule(const char *name) {
    char path[128];
    snprintf(path, sizeof(path), "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), name);
    int handle = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
    if (handle < 0) {
        snprintf(path, sizeof(path), "/system/common/lib/%s", name);
        handle = sceKernelLoadStartModule(path, 0, NULL, 0, NULL, NULL);
    }
    return handle;
}

static void *resolve(int *handle, const char *module, const char *symbol) {
    void *address = NULL;
    if (*handle == 0)
        *handle = loadModule(module);
    if (*handle < 0 || sceKernelDlsym(*handle, symbol, &address) != 0)
        return NULL;
    return address;
}

// LAZY_IMPORT(handle, module, ret, fail, name, params, args) defines `name` to forward to the real function,
// returning `fail` when the module or symbol is unavailable.
#define LAZY_IMPORT(handle, module, ret, fail, name, params, args)         \
    ret name params {                                                      \
        static ret (*real) params;                                         \
        if (!real)                                                         \
            real = (ret (*) params)resolve(&handle, module, #name);       \
        return real ? real args : (fail);                                  \
    }

int sceKernelDebugOutText(int, const char *);
int sceNetGetRandom(void *buffer, size_t size);

// LuaJIT's PS5 target seeds its PRNG with sceRandomGetRandomNumber and refuses to create a Lua state if that
// fails. libSceRandom isn't preloaded into a native title; when it can't be loaded, use libSceNet's generator,
// which is.
static int randomModule;
static int loadedRandomNumber(void *buffer, size_t size) {
    static int (*real)(void *, size_t);
    static int reported;
    if (!real)
        real = (int (*)(void *, size_t))resolve(&randomModule, "libSceRandom.sprx", "sceRandomGetRandomNumber");
    if (!reported) {
        char line[128];
        reported = 1;
        snprintf(line, sizeof(line), "[mm-ps5] random numbers from %s (libSceRandom handle %d)\n",
                 real ? "libSceRandom" : "sceNetGetRandom", randomModule);
        sceKernelDebugOutText(0, line);
    }
    return real ? real(buffer, size) : -1;
}

int sceRandomGetRandomNumber(void *buffer, size_t size) {
    if (loadedRandomNumber(buffer, size) == 0)
        return 0;
    return sceNetGetRandom(buffer, size) == 0 ? 0 : -1;
}

// Fills from the system's random number generator, which returns at most 64 bytes per call. Returns the
// number of bytes it could not fill.
static size_t fillRandom(unsigned char *out, size_t size) {
    while (size > 0) {
        size_t chunk = size < 64 ? size : 64;
        if (sceRandomGetRandomNumber(out, chunk) != 0)
            break;
        out += chunk;
        size -= chunk;
    }
    return size;
}

void arc4random_buf(void *buffer, size_t size) {
    unsigned char *out = buffer;
    size_t left = fillRandom(out, size);
    out += size - left;

    // Fallback: splitmix64 seeded from the clock. Not for cryptography.
    static unsigned long long state;
    if (left > 0 && state == 0)
        state = sceKernelGetProcessTime() ^ 0x9e3779b97f4a7c15ULL;
    while (left > 0) {
        unsigned long long z = (state += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        z ^= z >> 31;
        size_t chunk = left < sizeof(z) ? left : sizeof(z);
        memcpy(out, &z, chunk);
        out += chunk;
        left -= chunk;
    }
}

unsigned int arc4random(void) {
    unsigned int value;
    arc4random_buf(&value, sizeof(value));
    return value;
}

// Used by libc++'s std::random_device and OpenSSL. Unlike arc4random_buf, reports failure instead of
// falling back to a weak generator.
int getentropy(void *buffer, size_t size) {
    if (size > 256) {
        errno = EINVAL;
        return -1;
    }
    if (fillRandom(buffer, size) != 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}
