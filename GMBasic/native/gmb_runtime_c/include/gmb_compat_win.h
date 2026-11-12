/* gmb_compat_win.h — shim POSIX → Windows del runtime C (attivo SOLO su _WIN32/_WIN64).
 *
 * PERCHÉ ESISTE. Il mingw di zig non fornisce cinque funzioni che i sorgenti usano come POSIX:
 * `mkdir(path, mode)`, `strcasestr`, `lstat`, `timegm`, `mkstemps` (misurato con
 * `zig cc -target x86_64-windows-gnu`, 2026-11-12). Le varianti locali erano copiate in sette
 * file: qui c'è l'UNICA implementazione, e i sorgenti includono questo header.
 *
 * Si include SENZA guardie nei sorgenti: fuori da Windows non definisce nulla e non include
 * nulla, quindi il codice POSIX resta esattamente quello di prima.
 *
 * Nota: `struct stat`/`stat`/`S_ISDIR`/`opendir`/`readdir`/`nanosleep` NON sono shim — mingw li
 * ha, basta includere `<sys/stat.h>`, `<dirent.h>`, `<time.h>` nel sorgente che li usa.
 */
#ifndef GMB_COMPAT_WIN_H
#define GMB_COMPAT_WIN_H

#if defined(_WIN32) || defined(_WIN64)

#include <direct.h>     /* _mkdir */
#include <errno.h>      /* EINVAL/ENAMETOOLONG */
#include <io.h>         /* _close/_unlink */
#include <stdio.h>      /* rename/snprintf */
#include <stdlib.h>     /* mkstemp */
#include <string.h>     /* strlen/_strnicmp */
#include <sys/stat.h>   /* struct stat/stat/S_ISDIR */
#include <time.h>       /* _mkgmtime64 */

/* mkdir(path, mode): `_mkdir` ignora il modo (Windows non ha i permessi POSIX). */
#define mkdir(path, mode) _mkdir(path)

/* lstat: su Windows non ci sono i symlink POSIX da distinguere, `stat` è la scelta. */
#define lstat(path, st) stat((path), (st))

/* timegm: `_mkgmtime64` interpreta struct tm come UTC — stesso contratto della GNU. */
#define timegm(tm) ((time_t)_mkgmtime64(tm))

/* strcasestr (GNU): puntatore dentro `h`, NULL se assente. */
static inline char* gmb_strcasestr_win(const char* h, const char* n)
{
    size_t ln = strlen(n);
    if (ln == 0) return (char*)h;
    for (; *h; ++h)
        if (_strnicmp(h, n, ln) == 0) return (char*)h;
    return NULL;
}
#define strcasestr(h, n) gmb_strcasestr_win((h), (n))

/* mkstemps (GNU): `mkstemp` sul template senza suffisso, poi rinomina aggiungendolo. L'fd resta
 * valido (rename non cambia il file) e il template viene riscritto col nome finale, come la GNU. */
static inline int gmb_mkstemps_win(char* tpl, int suflen)
{
    char base[1024], final[1040];
    size_t n = strlen(tpl);
    if (suflen < 0 || (size_t)suflen > n) { errno = EINVAL; return -1; }
    if (n - (size_t)suflen >= sizeof base) { errno = ENAMETOOLONG; return -1; }
    memcpy(base, tpl, n - (size_t)suflen);
    base[n - (size_t)suflen] = '\0';
    int fd = mkstemp(base);
    if (fd < 0) return -1;
    snprintf(final, sizeof final, "%s%s", base, tpl + (n - (size_t)suflen));
    if (rename(base, final) != 0) { int e = errno; _close(fd); _unlink(base); errno = e; return -1; }
    snprintf(tpl, n + 1, "%s", final);
    return fd;
}
#define mkstemps(tpl, suflen) gmb_mkstemps_win((tpl), (suflen))

/* close: su Windows gli fd sono quelli del CRT. */
#define close(fd) _close(fd)

#endif /* _WIN32 || _WIN64 */

#endif /* GMB_COMPAT_WIN_H */
