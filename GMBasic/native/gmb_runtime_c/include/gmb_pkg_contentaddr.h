/* gmb_pkg_contentaddr.h — cache content-addressable (dedup dei file) del PM (riferimento C).
 * Blob per contenuto in <contentRoot>/<sha[:2]>/<sha>; i file della cache diventano link al blob
 * (hardlink → symlink → copia). Layout invariato: i file restano "veri" per il resto del codice. */
#ifndef GMB_PKG_CONTENTADDR_H
#define GMB_PKG_CONTENTADDR_H

#include "gmb_runtime.h"

/* Interna un file (lo memorizza come blob + lo sostituisce con un link). Scrive lo sha in out. 1/0. */
int gmb_pkg_content_intern(const char* contentRoot, const char* filePath, char* sha_out, size_t cap);

/* Interna tutti i file di una directory (ricorsivo). Ritorna il numero di file internati. */
int gmb_pkg_content_intern_dir(const char* contentRoot, const char* dir);

/* Rimuove i blob non più referenziati da nessun file della cache. Ritorna il numero rimosso. */
int gmb_pkg_content_gc(const char* contentRoot, const char* cacheRoot);

#endif /* GMB_PKG_CONTENTADDR_H */
