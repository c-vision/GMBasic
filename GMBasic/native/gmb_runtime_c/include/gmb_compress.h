/* ============================================================================
 * gmb_compress.h — Compressione reale via zlib (gzip/deflate/zlib).
 * ============================================================================ */
#ifndef GMB_COMPRESS_H
#define GMB_COMPRESS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
/* ZIP store-format: archivia file (ZIP(src[], dest)) ed estrae (UNZIP(zip, outdir)).
   Ritorna 1 su successo, 0 altrimenti. */
int gmb_zip_files(const char* const* files, int count, const char* dest);
/* Ritorno 0 = OK, -1 = errore (convenzione unica della famiglia gmb_zip_*). */
int gmb_unzip_files(const char* zip, const char* outdir);

/* Nome di entry ZIP SICURO (zip-slip): 1 = ok (scritto in `out`), 0 = entry da scartare
 * (path assoluto o componente ".."). Usata da gmb_unzip_files; esposta per i test. */
int gmb_zip_entry_safe(const char* name, char* out, size_t cap);

/* Compress data: algo 0=deflate, 1=gzip (con header), 2=zlib.
   Ritorna buffer malloc (chiamante libera con free) o NULL. */
unsigned char* gmb_compress_data(const char* algo, const unsigned char* data, size_t len, size_t* out_len);

/* Decompress data dello stesso formato. Ritorna malloc o NULL. */
unsigned char* gmb_decompress_data(const char* algo, const unsigned char* data, size_t len, size_t* out_len);

/* Algoritmi singoli (gzip/zlib/deflate/bzip2/brotli) */
unsigned char* gmb_compress_algo(int algo, const unsigned char* data, size_t len, size_t* out_len);
unsigned char* gmb_decompress_algo(int algo, const unsigned char* data, size_t len, size_t* out_len);

/* bzip2 / brotli (gmb_bzip2.c) */
unsigned char* gmb_bzip2_compress(const unsigned char* data, size_t len, size_t* out_len);
unsigned char* gmb_bzip2_decompress(const unsigned char* data, size_t len, size_t* out_len);
unsigned char* gmb_brotli_compress(const unsigned char* data, size_t len, size_t* out_len);
unsigned char* gmb_brotli_decompress(const unsigned char* data, size_t len, size_t* out_len);

/* Variante char* (ABI stabile per P/Invoke C#): testo/base64 in e out (malloc, gmb_compress_free).
 * algo: gzip/zlib/deflate/bzip2/brotli/zstd/lz4/xz. */
char* gmb_compress_b64(const char* algo, const char* text);
char* gmb_decompress_b64(const char* algo, const char* b64);
void  gmb_compress_free(char* p);

/* Decompressione per nome su buffer binario (malloc: free dal chiamante). Usata dal grep
 * per la ricerca trasparente dentro file compressi. algo: gzip/zlib/deflate/bzip2/brotli/
 * zstd/lz4/xz. Ritorna NULL se l'algoritmo non è disponibile o i dati sono invalidi. */
unsigned char* gmb_decompress_named(const char* algo, const unsigned char* d, size_t n, size_t* out_len);

/* Itera le entry di un archivio ZIP in memoria (STORE+DEFLATE), senza toccare il disco.
 * Per ogni file regolare invoca cb(name, content, clen, ctx). Ritorna il numero di entry. */
int gmb_zip_foreach_mem(const unsigned char* data, size_t len,
                        int (*cb)(const char* name, const unsigned char* content, size_t clen, void* ctx),
                        void* ctx);

#ifdef __cplusplus
}
#endif

#endif /* GMB_COMPRESS_H */

int gmb_zip_create(const char* dest, char* const* sources, int nsources);
int gmb_zip_extract(const char* archive, const char* dir);
int gmb_zip_list(const char* archive, char*** outNames, int* outCount);
void gmb_zip_list_free(char** names, int count);

/* COMPRESSION.ZIP con elenco di percorsi ';'-separati (parità PCode). */
int gmb_zip_files_semicolon(const char* paths, const char* dest);
