/* ============================================================================
 * gmb_hll.h — HyperLogLog (stima di cardinalità) — runtime condiviso (§0bis).
 *
 * Una sola implementazione per TUTTE le gambe: VM/C e C99Native la linkano
 * staticamente; le backends C# la rispecchiano (CommonLib/Collections/HyperLogLog.cs)
 * con lo STESSO contratto (xxHash64, stesso payload canonico, stessa formula di
 * stima, stesso formato di serializzazione) — verificato dal MiniTest di parità.
 *
 * Algoritmo: HyperLogLog classico (Flajolet et al.) con
 *   - registri a 8 bit (valori 0..q+1), m = 2^p (default p=14 → 16 KiB);
 *   - stima "raw" + **linear counting** per la zona di small-range;
 *   - correzione di large-range (collisioni dell'hash a 64 bit);
 *   - Count() arrotondato all'intero (la cardinalità È un intero: rende il
 *     risultato deterministico e stampabile in modo identico su tutte le gambe).
 *
 * Il payload canonico (contratto CONGELATO, `GMB_HLL_HASH_ID`): cambiarlo
 * invalida gli sketch persistiti.
 *   0x00 + (nulla)                          NIL
 *   0x01 + int64 little-endian              numerico INTERO (|v| ≤ 2^53)
 *   0x02 + 8 byte IEEE-754 little-endian    numerico NON intero
 *   0x03 + uint32 len + byte                STRINGA (byte grezzi)
 *   0x04 + 1 byte (0/1)                     BOOL
 *   0x05 + uint32 len + byte                altro (JSON canonico)
 * ============================================================================ */
#ifndef GMB_HLL_H
#define GMB_HLL_H

#include <stdint.h>
#include <stddef.h>
#include "gmb_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GMB_HLL_DEFAULT_PRECISION 14
#define GMB_HLL_MIN_PRECISION 4
#define GMB_HLL_MAX_PRECISION 18
#define GMB_HLL_HASH_ID 1u
/* Header di serializzazione: "GMBHLL1" (7) + p (1) + hash_id (1) = 9 byte. */
#define GMB_HLL_SERIAL_MAGIC "GMBHLL1"
#define GMB_HLL_SERIAL_MAGIC_LEN 7
#define GMB_HLL_SERIAL_HEADER_LEN 9

typedef struct gmb_hll gmb_hll_t;

/* Crea uno sketch con precisione p (clamp in [4,18]). NULL su OOM. */
gmb_hll_t* gmb_hll_new(int p);
void gmb_hll_free(gmb_hll_t* h);
void gmb_hll_clear(gmb_hll_t* h);
int  gmb_hll_precision(const gmb_hll_t* h);

/* xxHash64 (seed 0) — stessa implementazione in C# (contratto di hashing). */
uint64_t gmb_hll_hash_bytes(const void* data, size_t len);
/* Hash del payload canonico di un valore BASIC (vedi tabella sopra). */
uint64_t gmb_hll_hash_val(gmb_val_t v);

void gmb_hll_add_hash(gmb_hll_t* h, uint64_t hash);
void gmb_hll_add_val(gmb_hll_t* h, gmb_val_t v);

/* Stima della cardinalità (arrotondata all'intero) e errore relativo atteso. */
double gmb_hll_count(const gmb_hll_t* h);
double gmb_hll_error(const gmb_hll_t* h);

/* Unione registro-per-registro (max). 0 = ok, -1 = precisioni diverse. */
int gmb_hll_merge(gmb_hll_t* dst, const gmb_hll_t* src);

/* Serializzazione binaria: scrive in out (cap byte) e ritorna la lunghezza,
 * oppure -1 se il buffer è troppo piccolo. */
int gmb_hll_serialize(const gmb_hll_t* h, char* out, size_t cap);
/* Ricostruisce uno sketch; NULL se il formato non è valido. */
gmb_hll_t* gmb_hll_deserialize(const char* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* GMB_HLL_H */
