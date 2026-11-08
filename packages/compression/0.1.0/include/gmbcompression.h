/* gmbcompression.h — API del pacchetto `compression` (libgmbcompression).
 *
 * Espone SOLO le funzioni usate dai wrapper BASIC: testo → base64 compresso,
 * base64 → testo. ZIP/UNZIP NON sono qui: restano builtin del runtime
 * (gmb_zip_*), per non avere due implementazioni dello stesso archivio.
 *
 * Le stringhe di ritorno sono char* malloc'd (il bridge FFI le copia e libera).
 */
#ifndef GMBCOMPRESSION_H
#define GMBCOMPRESSION_H

#ifdef __cplusplus
extern "C" {
#endif

/* Compress: testo UTF-8 → base64 del flusso compresso.
 * algo: gzip | deflate | zlib | bzip2 | brotli | zstd | lz4 | xz (case-insensitive).
 * Ritorna NULL su errore/algoritmo non disponibile. Liberare con free(). */
char* ofx_cp_compress_b64(const char* text, const char* algo);

/* Decompress: base64 del flusso compresso → testo (NUL-terminato).
 * Ritorna NULL su base64 non valido o flusso corrotto. */
char* ofx_cp_decompress_b64(const char* b64, const char* algo);

/* Elenco algoritmi supportati (stringa costante, virgole). */
const char* ofx_cp_algorithms(void);

#ifdef __cplusplus
}
#endif

#endif /* GMBCOMPRESSION_H */
