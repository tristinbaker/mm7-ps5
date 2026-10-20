// Runtime pieces the static ps5-opengl SDK expects a title to provide. Taken from ps5-opengl v1.0.0's
// native-app/runtime_shims.c (GPL-3.0-or-later, Copyright (C) 2026 BlackBearReloaded), without its stdout
// redirection to /download0 and its hang after main returns: ps5_title.c handles both for this title.
// __assert is left to ps5_libc_compat.c.
#include <errno.h>
#include <stdio.h>

void ps5_opengl_glapi_tls_context_init(void) __asm__("_ZTH23_mesa_glapi_tls_Context");

void ps5_opengl_glapi_tls_context_init(void) {}

int mkstemps(char *template_name, int suffix_length) {
    (void)template_name;
    (void)suffix_length;
    errno = ENOSYS;
    return -1;
}

void openlog(const char *identifier, int option, int facility) {
    (void)identifier;
    (void)option;
    (void)facility;
}

FILE *popen(const char *command, const char *mode) {
    (void)command;
    (void)mode;
    errno = ENOSYS;
    return NULL;
}

int pclose(FILE *stream) {
    (void)stream;
    errno = ENOSYS;
    return -1;
}
