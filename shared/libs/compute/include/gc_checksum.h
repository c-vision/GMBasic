/*
 * gc_checksum.h — checksum generiche (CRC-32 IEEE e Adler-32).
 *
 * Promosse dal codice del libro MQL5 (CRC32.mqh, usato da PNG/zlib). Algoritmi
 * generali, indipendenti dal dominio: utili per integrita' dati e formati
 * container (zip/png/gzip). C99 puro.
 */
#ifndef GMBCOMPUTE_GC_CHECKSUM_H
#define GMBCOMPUTE_GC_CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CRC-32 (polinomio riflesso 0xEDB88320), init 0xFFFFFFFF, xor finale: standard
 * IEEE (zip/png/gzip). Ritorna il valore finalizzato. */
uint32_t gc_crc32(const uint8_t *data, size_t n);

/* Variante incrementale: `crc` e' il valore di lavoro (partire da 0xFFFFFFFF,
 * applicare ^0xFFFFFFFF alla fine). Utile per flussi a blocchi. */
uint32_t gc_crc32_update(uint32_t crc, const uint8_t *data, size_t n);

/* Adler-32 (RFC 1950): checksum rapida usata da zlib/png. Init 1. */
uint32_t gc_adler32(const uint8_t *data, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_CHECKSUM_H */
