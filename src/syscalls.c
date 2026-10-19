/* newlib-nano utilise un petit tas pour convertir les nombres réels dans
 * strtod et snprintf. L'application n'alloue aucun objet elle-même.
 * Le tas est explicitement borné par le linker : jamais d'empiètement sur la pile. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
extern unsigned char _heap_start, _heap_end;
void *_sbrk(ptrdiff_t increment) {
    static unsigned char *current;
    if (!current)
        current = &_heap_start;
    uintptr_t start = (uintptr_t)&_heap_start;
    uintptr_t end = (uintptr_t)&_heap_end;
    uintptr_t at = (uintptr_t)current;
    if ((increment >= 0 && (uintptr_t)increment > end - at) ||
        (increment < 0 && (uintptr_t)(-(increment + 1)) + 1 > at - start)) {
        errno = ENOMEM;
        return (void *)-1;
    }
    unsigned char *previous = current;
    current += increment;
    return previous;
}

/* Compléments stdio nécessaires au printf conservé dans le pilote SD officiel.
 * Ils décrivent un terminal série, pas un système de fichiers POSIX. */
#include "board.h"
#include <sys/stat.h>
#include <string.h>
int _fstat(int fd, struct stat *st) {
    if (fd < 0 || fd > 2 || !st) {
        errno = EBADF;
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR;
    return 0;
}
int _isatty(int fd) {
    if (fd >= 0 && fd <= 2)
        return 1;
    errno = EBADF;
    return 0;
}
int _close(int fd) {
    if (fd >= 0 && fd <= 2)
        return 0;
    errno = EBADF;
    return -1;
}
int _lseek(int fd, int offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}
int _read(int fd, char *out, int len) {
    if (fd != 0 || !out || len < 0) {
        errno = EBADF;
        return -1;
    }
    int n = 0, c;
    while (n < len && (c = Board_ConsoleGet()) >= 0)
        out[n++] = (char)c;
    if (!n && len) {
        errno = EAGAIN;
        return -1;
    }
    return n;
}
int _getpid(void) {
    return 1;
}
int _kill(int pid, int signal) {
    (void)pid;
    (void)signal;
    errno = ESRCH;
    return -1;
}
