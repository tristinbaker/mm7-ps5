// Arch Linux builds compiler-rt's builtins with _FORTIFY_SOURCE, so its emutls code calls glibc's
// checked helpers. The console's libc does not export them, so provide the ones that get referenced.
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void *__memset_chk(void *destination, int value, size_t length, size_t destinationLength) {
    if (length > destinationLength)
        abort();
    return memset(destination, value, length);
}

void *__memcpy_chk(void *destination, const void *source, size_t length, size_t destinationLength) {
    if (length > destinationLength)
        abort();
    return memcpy(destination, source, length);
}
