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

// The DualSense layout, written as the [gamepad] section of openenroth.ini (see docs/INSTALL.md for the table).
// Every action is listed, so none of OpenEnroth's desktop defaults, some of which share a button, are left over.
// Bump GAMEPAD_LAYOUT_VERSION when changing it: the new layout then replaces the bindings in existing configs once.
#define GAMEPAD_LAYOUT_VERSION "1"
static const char gamepadLayout[] =
    "[gamepad]\n"
    "forward = LSTICK UP\n"
    "backward = LSTICK DOWN\n"
    "step_left = LSTICK LEFT\n"
    "step_right = LSTICK RIGHT\n"
    "left = RSTICK LEFT\n"
    "right = RSTICK RIGHT\n"
    "look_up = RSTICK UP\n"
    "look_down = RSTICK DOWN\n"
    "always_run = L3\n"
    "event_trigger = A (CROSS)\n"
    "escape = B (CIRCLE)\n"
    "attack = X (SQUARE)\n"
    "jump = Y (TRIANGLE)\n"
    "fly_up = Y (TRIANGLE)\n"
    "fly_down = L2\n"
    "land = \n"
    "cast = L1\n"
    "cast_ready = R1\n"
    "combat = R2\n"
    "pass = R3\n"
    "inventory = START\n"
    "rest = BACK\n"
    "char_cycle = DPAD DOWN\n"
    "auto_notes = DPAD UP\n"
    "map_book = DPAD LEFT\n"
    "quest = DPAD RIGHT\n"
    "yell = \n"
    "center_view = \n"
    "history = \n"
    "time_calendar = \n"
    "quick_save = \n"
    "quick_load = \n"
    "quick_reference = \n"
    "zoom_in = \n"
    "zoom_out = \n"
    "screenshot = \n"
    "console = \n"
    "toggle_window_mode = \n"
    "select_char_1 = \n"
    "select_char_2 = \n"
    "select_char_3 = \n"
    "select_char_4 = \n"
    "select_npc_1 = \n"
    "select_npc_2 = \n"
    // Menus and dialogs.
    "dialog_up = DPAD UP\n"
    "dialog_down = DPAD DOWN\n"
    "dialog_left = DPAD LEFT\n"
    "dialog_right = DPAD RIGHT\n"
    "dialog_select = A (CROSS)\n"
    "yes = A (CROSS)\n"
    "no = B (CIRCLE)\n"
    "return = A (CROSS)\n"
    "return_to_game = B (CIRCLE)\n"
    "new_game = A (CROSS)\n"
    "load_game = X (SQUARE)\n"
    "save_game = Y (TRIANGLE)\n"
    "credits = Y (TRIANGLE)\n"
    "exit_game = B (CIRCLE)\n"
    "options = R1\n"
    "controls = L1\n"
    "clear = Y (TRIANGLE)\n"
    "minus = L1\n"
    "plus = R1\n"
    "stats = A (CROSS)\n"
    "skills = X (SQUARE)\n"
    "awards = L1\n"
    "rest_8_hours = BACK\n"
    "wait_till_dawn = Y (TRIANGLE)\n"
    "wait_hour = X (SQUARE)\n"
    "wait_5_minutes = A (CROSS)\n";

static const char configDirectory[] = "/download0/.openenroth";
static const char configPath[] = "/download0/.openenroth/openenroth.ini";
static const char layoutVersionPath[] = "/download0/.openenroth/ps5-gamepad-layout";

static char *readFile(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *text = size >= 0 ? malloc((size_t)size + 1) : NULL;
    if (text) {
        size_t read = fread(text, 1, (size_t)size, file);
        text[read] = '\0';
    }
    fclose(file);
    return text;
}

static int writeFile(const char *path, const char *first, size_t firstLength, const char *second) {
    char temporary[128];
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    FILE *file = fopen(temporary, "wb");
    if (!file)
        return -1;
    int ok = fwrite(first, 1, firstLength, file) == firstLength && fputs(second, file) >= 0;
    ok = fclose(file) == 0 && ok;
    return ok ? rename(temporary, path) : -1;
}

// First launch: write the console defaults. Later launches: if the layout version changed, replace the config's
// [gamepad] section with the current layout and keep everything else.
static void prepareConfig(void) {
    mkdir(configDirectory, 0777);
    char *version = readFile(layoutVersionPath);
    int layoutCurrent = version && strcmp(version, GAMEPAD_LAYOUT_VERSION) == 0;
    free(version);

    char *config = readFile(configPath);
    int result = 0;
    if (!config) {
        result = writeFile(configPath, defaultConfig, strlen(defaultConfig), gamepadLayout);
    } else if (!layoutCurrent) {
        // Drop the old [gamepad] section: from its header to the next section header.
        char *start = strstr(config, "[gamepad]");
        while (start && start != config && start[-1] != '\n')
            start = strstr(start + 1, "[gamepad]");
        size_t keptLength = strlen(config);
        if (start) {
            char *end = strstr(start + 1, "\n[");
            size_t tail = end ? strlen(end + 1) : 0;
            memmove(start, end ? end + 1 : start + strlen(start), tail + 1);
            keptLength = strlen(config);
        }
        if (keptLength > 0 && config[keptLength - 1] != '\n')
            config[keptLength++] = '\n'; // There is room: the buffer held at least the removed header.
        result = writeFile(configPath, config, keptLength, gamepadLayout);
    }
    free(config);

    if (result != 0) {
        fprintf(stderr, "[mm-ps5] could not write %s: %s\n", configPath, strerror(errno));
        return;
    }
    if (!layoutCurrent) {
        FILE *file = fopen(layoutVersionPath, "w");
        if (file) {
            fputs(GAMEPAD_LAYOUT_VERSION, file);
            fclose(file);
        }
        fprintf(stderr, "[mm-ps5] applied gamepad layout " GAMEPAD_LAYOUT_VERSION " to %s\n", configPath);
    }
}

__attribute__((constructor)) static void ps5TitleStartup(void) {
    setenv("HOME", "/download0", 1);
    setenv("OPENENROTH_MM7_PATH", "/app0/assets/mm7", 1);
    sceKernelDebugOutText(0, "[mm-ps5] OpenEnroth title starting\n");
    prepareConfig();
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
