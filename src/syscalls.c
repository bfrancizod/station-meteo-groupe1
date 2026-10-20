// Rôle : Adapte les services minimaux de la bibliothèque C à la mémoire et à la console STM32.
// Adaptations minimales de la bibliothèque C au STM32 : tas et terminal série.
// linker.ld fixe les limites du tas ; setup.c fournit l'émission _write utilisée par printf.
/* newlib-nano utilise un petit tas pour convertir les nombres réels dans
 * strtod et snprintf. L'application n'alloue aucun objet elle-même.
 * Le tas est explicitement borné par l'éditeur de liens : jamais d'empiètement sur la pile. */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
// Symboles fournis par l'éditeur de liens : leurs adresses délimitent la mémoire disponible.
extern unsigned char _heap_start, _heap_end;
// Demandée par l'allocateur C : déplace la limite du tas de increment octets.
// Renvoie l'ancienne limite, ou (void *)-1 et errno si la demande dépasse les bornes.
void *_sbrk(ptrdiff_t increment) {
    static unsigned char *current;
    if (!current)
        current = &_heap_start;
    uintptr_t start = (uintptr_t)&_heap_start;
    uintptr_t end = (uintptr_t)&_heap_end;
    uintptr_t at = (uintptr_t)current;
    // Éviter de calculer directement -increment permet de traiter le plus petit entier signé.
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
// Remplit les attributs d'un descripteur 0..2 comme terminal ; 0 = succès, -1 = erreur.
int _fstat(int fd, struct stat *st) {
    if (fd < 0 || fd > 2 || !st) {
        errno = EBADF;
        return -1;
    }
    memset(st, 0, sizeof(*st));
    st->st_mode = S_IFCHR;
    return 0;
}
// Indique si fd représente entrée/sortie/erreur standard ; aucun fichier SD n'est ouvert ici.
int _isatty(int fd) {
    if (fd >= 0 && fd <= 2)
        return 1;
    errno = EBADF;
    return 0;
}
// Accepte la fermeture logique de 0..2 sans arrêter l'UART ; rejette les autres descripteurs.
int _close(int fd) {
    if (fd >= 0 && fd <= 2)
        return 0;
    errno = EBADF;
    return -1;
}
// Un flux série n'a pas de position de fichier : tout déplacement renvoie ESPIPE.
int _lseek(int fd, int offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    errno = ESPIPE;
    return -1;
}
// Copie au plus len octets de l'entrée console vers out, sans attendre.
// Renvoie leur nombre, ou -1/EAGAIN si rien n'est disponible ; aucune terminaison '\0' ajoutée.
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
// Identifiant conventionnel attendu par newlib ; il n'existe pas de processus ici.
int _getpid(void) {
    return 1;
}
// Les signaux de processus ne sont pas implémentés : renvoie toujours -1/ESRCH.
int _kill(int pid, int signal) {
    (void)pid;
    (void)signal;
    errno = ESRCH;
    return -1;
}
