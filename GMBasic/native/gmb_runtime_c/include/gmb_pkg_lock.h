/* ============================================================================
 * gmb_pkg_lock.h — lockfile del Package Manager + integrità (sha256 per-file).
 * Riferimento C (§"il backend di riferimento è C"); il C# lo consuma via P/Invoke.
 *
 * Formato gmb-package.lock (righe '#' = commento):
 *   name@version|contentType|backend|platform|sha256(file:hash,...)
 * ============================================================================ */
#ifndef GMB_PKG_LOCK_H
#define GMB_PKG_LOCK_H

#include "gmb_runtime.h"

typedef struct gmb_pkg_lock gmb_pkg_lock_t;

/* Legge un lockfile. NULL se non esiste o vuoto. */
gmb_pkg_lock_t* gmb_pkg_lock_read(const char* path);
void gmb_pkg_lock_free(gmb_pkg_lock_t* l);

int gmb_pkg_lock_count(gmb_pkg_lock_t* l);

/* i-esima voce: copia name/version/contentType/backend/platform. Ritorna 1/0. */
int gmb_pkg_lock_entry(gmb_pkg_lock_t* l, int i,
                       char* name, size_t ncap, char* version, size_t vcap,
                       char* ct, size_t ccap, char* backend, size_t bcap,
                       char* platform, size_t pcap);

/* Numero di hash della i-esima voce; j-esimo (file, hash). */
int gmb_pkg_lock_hash_count(gmb_pkg_lock_t* l, int i);
int gmb_pkg_lock_hash(gmb_pkg_lock_t* l, int i, int j, char* file, size_t fcap, char* hash, size_t hcap);

/* Scrive il lock canonico (header + body) in `path`. 1/0. */
int gmb_pkg_lock_write(const char* path, const char* body);

/* Scrive il lock canonico; se il contenuto differisce dal lock esistente ne archivia una copia
 * in <project_dir>/.gmbasic/lock-history/<timestamp>.lock (per `gmb rollback`). 1/0. */
int gmb_pkg_lock_write_with_history(const char* project_dir, const char* lock_path, const char* body);

/* sha256 esadecimale (64 char) di un file. 1/0 (0 se il file non esiste). */
int gmb_pkg_sha256_file(const char* path, char* out, size_t cap);

/* Fingerprint per-file di un pacchetto installato (manifest+exports+binaries+ffi+samples),
 * "file:hash,..." ordinate per file. 1/0. */
int gmb_pkg_pkg_hash_files(const char* installDir, const char* manifestJson, char* out, size_t cap);

/* Verifica l'integrità della cache vs il lock: per ogni voce con hash confronta i file in
 * <cacheRoot>/<name>/<version>/. Scrive un report (una riga per problema) in `out` e ritorna
 * il numero di problemi (0 = integro). */
int gmb_pkg_lock_verify(gmb_pkg_lock_t* l, const char* cacheRoot, char* out, size_t cap);

#endif /* GMB_PKG_LOCK_H */
