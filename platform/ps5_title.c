// Startup and runtime glue for the OpenEnroth PS5 native title.
//
// - Points OpenEnroth at the game data packaged in the title (/app0/assets/mm7) and at the title's own
//   writable storage for config, saves and logs (/download0/.openenroth).
// - Writes a console-friendly openenroth.ini on first launch.
// - Sizes the GL runtime's heap (native-app/app_heap.c) for OpenEnroth.
// - Closes the app through the system on exit: a native title may not exit the process itself (the exit
//   syscall raises SIGSYS), the same finding and fix as icepuente/rct2-ps5.
// - Defines libc functions that only libScePosixForWebKit exports. That module isn't loaded into a native
//   title, so imports of them would be null at run time (see scripts/check-imports.sh).
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int sceKernelDebugOutText(int channel, const char *text);
int sceKernelUsleep(unsigned int microseconds);
int sceSystemServiceLoadExec(const char *path, const char *const *arguments);

// OpenEnroth keeps decoded textures, sprites and level data in CPU memory. Mapped from direct memory by
// app_heap.c; it falls back to smaller sizes and then to the libc heap if this much isn't available.
const size_t ps5_opengl_heap_size = (size_t)1536 << 20;

// Written as openenroth.ini on first launch. The PS5's SDL2 video driver supports exactly one window, sized to
// the display mode and not resizable. OpenEnroth fills in everything else with its defaults.
static const char defaultConfig[] =
    "[window]\n"
    "display = 0\n"
    "mode = fullscreen_borderless\n"
    "width = 1920\n"
    "height = 1080\n"
    "position_x = 0\n"
    "position_y = 0\n"
    "resizable = false\n"
    "mouse_grab = false\n"
    "\n"
    "[graphics]\n"
    "vsync = true\n"
    "fps_limit = 60\n"
    "\n"
    "[debug]\n"
    "no_intro = true\n"
    "no_logo = true\n";

static void seedDefaultConfig(void) {
    const char *directory = "/download0/.openenroth";
    const char *path = "/download0/.openenroth/openenroth.ini";
    if (access(path, F_OK) == 0)
        return;
    mkdir(directory, 0777);
    FILE *file = fopen(path, "w");
    if (!file) {
        fprintf(stderr, "[mm-ps5] could not write %s: %s\n", path, strerror(errno));
        return;
    }
    fputs(defaultConfig, file);
    fclose(file);
    fprintf(stderr, "[mm-ps5] wrote default %s\n", path);
}

__attribute__((constructor)) static void ps5TitleStartup(void) {
    setenv("HOME", "/download0", 1);
    setenv("OPENENROTH_MM7_PATH", "/app0/assets/mm7", 1);
    sceKernelDebugOutText(0, "[mm-ps5] OpenEnroth title starting\n");
    seedDefaultConfig();
}

static __attribute__((noreturn)) void closeApplication(int status) {
    char line[64];
    fflush(NULL);
    snprintf(line, sizeof(line), "[mm-ps5] exit(%d), closing the application\n", status);
    sceKernelDebugOutText(0, line);
    sceSystemServiceLoadExec("exit", NULL);
    for (;;)
        sceKernelUsleep(100000);
}

// Called by the native app template's CRT when main returns.
void catchReturnFromMain(int status) {
    closeApplication(status);
}

// exit() is linked with --wrap=exit, see scripts/package.sh.
void __wrap_exit(int status) {
    closeApplication(status);
}

// Not exported to a native title at all.
struct tm *gmtime_r(const time_t *time, struct tm *result) {
    struct tm *shared = gmtime(time);
    if (!shared)
        return NULL;
    *result = *shared;
    return result;
}

FILE *tmpfile(void) {
    errno = ENOSYS;
    return NULL;
}

size_t fwrite_unlocked(const void *data, size_t size, size_t count, FILE *stream) {
    return fwrite(data, size, count, stream);
}

// MB_CUR_MAX: the title only ever runs in the "C" locale (see ps5_libc_compat.c).
int ___mb_cur_max(void) {
    return 1;
}

// Referenced by OpenSSL code that FFmpeg's TLS protocol drags in (async jobs, DTLS, its DSO loader) and that
// OpenEnroth never runs. Fail loudly if that ever changes.
static __attribute__((noreturn)) void unsupported(const char *function) {
    char line[128];
    snprintf(line, sizeof(line), "[mm-ps5] unsupported function called: %s\n", function);
    sceKernelDebugOutText(0, line);
    abort();
}

int _setjmp(void *environment) {
    (void)environment;
    unsupported("_setjmp");
}

void _longjmp(void *environment, int value) {
    (void)environment;
    (void)value;
    unsupported("_longjmp");
}

int sendmmsg(int socket, void *messages, unsigned int length, int flags) {
    (void)socket;
    (void)messages;
    (void)length;
    (void)flags;
    unsupported("sendmmsg");
}

int recvmmsg(int socket, void *messages, unsigned int length, int flags, void *timeout) {
    (void)socket;
    (void)messages;
    (void)length;
    (void)flags;
    (void)timeout;
    unsupported("recvmmsg");
}

// Name resolution, referenced by FFmpeg's and OpenSSL's network code. OpenEnroth makes no network requests,
// so report failure rather than leaving null imports from libScePosixForWebKit.
#define EAI_FAIL_VALUE 4 // EAI_FAIL in FreeBSD's netdb.h.

int getaddrinfo(const char *node, const char *service, const void *hints, void **result) {
    (void)node;
    (void)service;
    (void)hints;
    *result = NULL;
    return EAI_FAIL_VALUE;
}

void freeaddrinfo(void *info) {
    (void)info;
}

int getnameinfo(const void *address, unsigned int addressLength, char *host, size_t hostLength, char *service,
                size_t serviceLength, int flags) {
    (void)address;
    (void)addressLength;
    (void)host;
    (void)hostLength;
    (void)service;
    (void)serviceLength;
    (void)flags;
    return EAI_FAIL_VALUE;
}

const char *gai_strerror(int error) {
    (void)error;
    return "name resolution is not available in this title";
}

void *gethostbyname(const char *name) {
    (void)name;
    return NULL;
}

int dladdr(const void *address, void *info) {
    (void)address;
    (void)info;
    return 0; // "Not found", OpenSSL then works without the module path.
}

int isatty(int descriptor) {
    (void)descriptor;
    errno = ENOTTY;
    return 0;
}

char *strcasestr(const char *haystack, const char *needle) {
    size_t length = strlen(needle);
    for (; *haystack; haystack++)
        if (strncasecmp(haystack, needle, length) == 0)
            return (char *)haystack;
    return length == 0 ? (char *)haystack : NULL;
}

int mkstemp(char *pattern) {
    static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    size_t length = strlen(pattern);
    if (length < 6 || strcmp(pattern + length - 6, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }
    for (int attempt = 0; attempt < 100; attempt++) {
        for (size_t i = length - 6; i < length; i++)
            pattern[i] = letters[(size_t)rand() % (sizeof(letters) - 1)];
        int descriptor = open(pattern, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (descriptor >= 0 || errno != EEXIST)
            return descriptor;
    }
    errno = EEXIST;
    return -1;
}
