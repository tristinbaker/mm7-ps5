// Console output and sysctl for a native title. Adapted from icepuente/rct2-ps5 platform/ps5_native_shims.c
// at commit 115c231c9f (GPL-3.0-or-later).
//
// A native title has no stdout or stderr, so writes to them go to the kernel log (klogsrv streams it on port
// 3232), one line at a time and synchronously, so that the last lines before a crash aren't lost. printf and
// friends are defined here; vfprintf, fputs, fputc, fwrite and fflush are wrapped (see
// scripts/package.sh), so other streams still work.
//
// The sandbox refuses KERN_PROC_PATHNAME, which libraries use to find their own executable; answer it with the
// title's eboot and pass every other query through.
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sysctl.h>

int sceKernelDebugOutText(int, const char *);
int __real_vfprintf(FILE *, const char *, va_list);
int __real_fputs(const char *, FILE *);
int __real_fputc(int, FILE *);
size_t __real_fwrite(const void *, size_t, size_t, FILE *);
int __real_fflush(FILE *);
int __real_sysctl(const int *, unsigned int, void *, size_t *, const void *, size_t);

static pthread_mutex_t logLock = PTHREAD_MUTEX_INITIALIZER;
static char logLine[1024];
static size_t logLength;

static void logFlushLocked(void) {
    if (logLength > 0) {
        logLine[logLength] = '\0';
        sceKernelDebugOutText(0, logLine);
        logLength = 0;
    }
}

static void logWrite(const char *text, size_t length) {
    pthread_mutex_lock(&logLock);
    for (size_t i = 0; i < length; i++) {
        logLine[logLength++] = text[i];
        if (text[i] == '\n' || logLength == sizeof(logLine) - 1)
            logFlushLocked();
    }
    pthread_mutex_unlock(&logLock);
}

static int isConsole(FILE *stream) {
    return stream == stdout || stream == stderr;
}

int vprintf(const char *format, va_list arguments) {
    char buffer[1024];
    int count = vsnprintf(buffer, sizeof(buffer), format, arguments);
    logWrite(buffer, count < 0 ? 0 : (size_t)count < sizeof(buffer) ? (size_t)count : sizeof(buffer) - 1);
    return count;
}

int printf(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int count = vprintf(format, arguments);
    va_end(arguments);
    return count;
}

int puts(const char *text) {
    logWrite(text, strlen(text));
    logWrite("\n", 1);
    return 0;
}

#undef putchar
int putchar(int character) {
    char c = (char)character;
    logWrite(&c, 1);
    return character;
}

int __wrap_vfprintf(FILE *stream, const char *format, va_list arguments) {
    return isConsole(stream) ? vprintf(format, arguments) : __real_vfprintf(stream, format, arguments);
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    int count = __wrap_vfprintf(stream, format, arguments);
    va_end(arguments);
    return count;
}

int __wrap_fputs(const char *text, FILE *stream) {
    if (!isConsole(stream))
        return __real_fputs(text, stream);
    logWrite(text, strlen(text));
    return 0;
}

int __wrap_fputc(int character, FILE *stream) {
    if (!isConsole(stream))
        return __real_fputc(character, stream);
    char c = (char)character;
    logWrite(&c, 1);
    return character;
}

size_t __wrap_fwrite(const void *data, size_t size, size_t count, FILE *stream) {
    if (!isConsole(stream))
        return __real_fwrite(data, size, count, stream);
    logWrite(data, size * count);
    return count;
}

int __wrap_fflush(FILE *stream) {
    if (!stream || isConsole(stream)) {
        pthread_mutex_lock(&logLock);
        logFlushLocked();
        pthread_mutex_unlock(&logLock);
        if (stream)
            return 0;
    }
    return __real_fflush(stream);
}

int __wrap_sysctl(const int *name, unsigned int nameLength, void *oldValue, size_t *oldLength, const void *newValue,
                  size_t newLength) {
    static const char executablePath[] = "/app0/eboot.bin";
    if (nameLength == 4 && name[0] == CTL_KERN && name[1] == KERN_PROC && name[2] == KERN_PROC_PATHNAME && !newValue) {
        if (oldValue) {
            if (!oldLength || *oldLength < sizeof(executablePath)) {
                errno = ENOMEM;
                return -1;
            }
            memcpy(oldValue, executablePath, sizeof(executablePath));
        }
        if (oldLength)
            *oldLength = sizeof(executablePath);
        return 0;
    }
    return __real_sysctl(name, nameLength, oldValue, oldLength, newValue, newLength);
}
