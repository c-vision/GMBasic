/* dirent.h (Windows) — implementazione minima di `opendir/readdir/closedir` su Win32.
 *
 * Le nostre librerie e il runtime usano `<dirent.h>` in 67 punti (scansione di cartelle):
 * `DIR*`, `struct dirent.d_name`, `d_type` (DT_DIR/DT_REG), `readdir` che ritorna NULL a fine
 * scansione. Windows non ce l'ha: qui si mappa su `FindFirstFileA`/`FindNextFileA`.
 *
 * Header-only con funzioni `static`: ogni unità di compilazione ha la sua copia (sono poche righe,
 * e così non serve un .c in più nel runtime). Niente stato globale condiviso.
 */
#ifndef GMB_WIN32_DIRENT_H
#define GMB_WIN32_DIRENT_H
#if defined(_WIN32) || defined(_WIN64)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef DT_UNKNOWN
#define DT_UNKNOWN 0
#endif
#ifndef DT_DIR
#define DT_DIR 4
#endif
#ifndef DT_REG
#define DT_REG 8
#endif

struct dirent {
    char d_name[MAX_PATH];
    unsigned char d_type;
};

typedef struct gmb_win_DIR {
    HANDLE h;
    WIN32_FIND_DATAA fd;
    struct dirent ent;
    int first;
    char pattern[MAX_PATH * 2];   /* serve a `rewinddir` (FindFirstFile non si riavvolge) */
} DIR;

static inline DIR* opendir(const char* path)
{
    if (!path || !*path) return NULL;
    char pattern[MAX_PATH * 2];
    /* il pattern di FindFirstFile vuole il jolly finale */
    snprintf(pattern, sizeof pattern, "%s%s*", path,
             (path[strlen(path) - 1] == '/' || path[strlen(path) - 1] == '\\') ? "" : "\\");
    DIR* d = (DIR*)calloc(1, sizeof(DIR));
    if (!d) return NULL;
    snprintf(d->pattern, sizeof d->pattern, "%s", pattern);
    d->h = FindFirstFileA(pattern, &d->fd);
    if (d->h == INVALID_HANDLE_VALUE) { free(d); return NULL; }
    d->first = 1;
    return d;
}

static inline struct dirent* readdir(DIR* d)
{
    if (!d || d->h == INVALID_HANDLE_VALUE) return NULL;
    if (!d->first) {
        if (!FindNextFileA(d->h, &d->fd)) return NULL;
    }
    d->first = 0;
    snprintf(d->ent.d_name, sizeof d->ent.d_name, "%s", d->fd.cFileName);
    d->ent.d_type = (d->fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? DT_DIR : DT_REG;
    return &d->ent;
}

static inline int closedir(DIR* d)
{
    if (!d) return -1;
    if (d->h != INVALID_HANDLE_VALUE) FindClose(d->h);
    free(d);
    return 0;
}

static inline void rewinddir(DIR* d)
{
    if (!d) return;
    if (d->h != INVALID_HANDLE_VALUE) FindClose(d->h);
    d->h = FindFirstFileA(d->pattern, &d->fd);
    d->first = 1;
}

#endif /* _WIN32 */
#endif /* GMB_WIN32_DIRENT_H */
