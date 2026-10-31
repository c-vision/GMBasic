/* gmb_fsutil.h — utilità di filesystem condivise dal runtime (una sola implementazione).
 *
 * Erano copie locali in gmb_libs_fetch.c / gmb_fetch.c / gmb_brains.c (mkdir -p, rmtree,
 * isdir). Chi ne ha bisogno (cervelli, fixture, download) usa QUESTE.
 */
#ifndef GMB_FSUTIL_H
#define GMB_FSUTIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int  gmb_fs_isdir(const char* path);
int  gmb_fs_isfile(const char* path);

/* mkdir -p. Ritorna 1 se alla fine `path` è una directory. */
int  gmb_fs_mkdir_p(const char* path);

/* Rimozione ricorsiva (file o cartella). NON segue i symlink: rimuove il link, non il target. */
void gmb_fs_rmtree(const char* path);

/* Byte liberi nella filesystem che contiene `dir`; (unsigned long long)-1 se non determinabile. */
unsigned long long gmb_fs_free_bytes(const char* dir);

#ifdef __cplusplus
}
#endif

#endif /* GMB_FSUTIL_H */
