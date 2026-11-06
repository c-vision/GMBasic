/*
 * gc_compress.h — compressione Huffman (da v-x-zhang/quaml).
 *
 * Codifica di Huffman: codebook (simbolo -> codice prefix-free) + bitstream,
 * con round-trip esatto. Pipeline delta+Huffman per serie temporali di mercato
 * (base = primo valore, bitstream = Huffman dei delta).
 */
#ifndef GMBCOMPUTE_GC_COMPRESS_H
#define GMBCOMPUTE_GC_COMPRESS_H

#include "gc_core.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Voce del codebook Huffman: simbolo, codice (bit MSB-first) e lunghezza. */
typedef struct { int32_t sym; uint64_t code; int len; } gc_huff_symbol;

/* Comprime campioni int32 con Huffman: codebook allocato in *codes (nsym
 * simboli) e bitstream in *bits (nbits bit). Le allocazioni vanno liberate
 * dal chiamante. Ritorna 0 se OK, -1 su errore. */
int gc_huffman_compress(const int32_t *samples, int n,
                        gc_huff_symbol **codes, int *nsym,
                        uint8_t **bits, int *nbits);

/* Decodifica: ricostruisce l'albero dal codebook (trie) e decodifica n
 * simboli dal bitstream. out[] lunghezza n. Ritorna 0 se OK, -1 su errore. */
int gc_huffman_decompress(const uint8_t *bits, int nbits,
                          const gc_huff_symbol *codes, int nsym,
                          int32_t *out, int n);

/* Pipeline delta+Huffman (market-data): base = primo campione, i delta sono
 * Huffman-comprimati. Ritorna 0/-1; base_out riceve il primo valore. */
int gc_huffman_compress_delta(const int32_t *samples, int n,
                              gc_huff_symbol **codes, int *nsym,
                              uint8_t **bits, int *nbits, int32_t *base_out);
int gc_huffman_decompress_delta(const uint8_t *bits, int nbits,
                                const gc_huff_symbol *codes, int nsym,
                                int32_t base, int32_t *out, int n);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPUTE_GC_COMPRESS_H */