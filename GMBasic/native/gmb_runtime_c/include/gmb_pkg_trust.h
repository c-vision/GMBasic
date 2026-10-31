/* ============================================================================
 * gmb_pkg_trust.h — keyring + firme HMAC del Package Manager (riferimento C).
 * Il C# consuma via P/Invoke. HMAC-SHA256 nativo (gmb_hmac_sha256); Ed25519
 * pubblico via monocypher (fase successiva).
 *
 * Keyring: file JSON { "<nome>": "<chiave>" } (es. ~/.gmbasic/trusted-keys.json).
 * ============================================================================ */
#ifndef GMB_PKG_TRUST_H
#define GMB_PKG_TRUST_H

#include "gmb_runtime.h"

/* HMAC-SHA256 esadecimale (64 char) di un buffer. 1/0. */
int gmb_pkg_hmac_hex(const char* key, const unsigned char* data, size_t len, char* out, size_t cap);

/* HMAC-SHA256 esadecimale del CONTENUTO di un file (verifica di una firma detached). 1/0. */
int gmb_pkg_hmac_file_hex(const char* key, const char* file, char* out, size_t cap);

/* Fingerprint SHA256 (16 hex) di una chiave pubblica/segreta (identificatore compatto). */
int gmb_pkg_key_fingerprint(const char* key, char* out, size_t cap);

/* Keyring: valore di `name` ("" se assente). 1 se trovato, 0 altrimenti. */
int gmb_pkg_keyring_get(const char* keyringPath, const char* name, char* out, size_t cap);

/* Keyring: imposta/aggiorna una voce (crea il file se manca). 1/0. */
int gmb_pkg_keyring_set(const char* keyringPath, const char* name, const char* key);

/* Keyring: rimuove una voce. 1 se rimossa, 0 se assente/errore. */
int gmb_pkg_keyring_remove(const char* keyringPath, const char* name);

/* Keyring: numero di voci; i-esima (name, key) per indice. 1/0. */
int gmb_pkg_keyring_count(const char* keyringPath);
int gmb_pkg_keyring_item(const char* keyringPath, int i, char* name, size_t ncap, char* key, size_t kcap);

#endif /* GMB_PKG_TRUST_H */
