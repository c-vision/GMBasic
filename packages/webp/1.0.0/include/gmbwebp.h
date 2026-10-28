/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * gmbwebp.h — API pubblica dello shim FFI del pacchetto `webp` (WebP via libwebp statica). */
#ifndef GMBWEBP_H
#define GMBWEBP_H

#ifdef __cplusplus
extern "C" {
#endif

/* 1 = shim costruito con libwebp. */
int gmbwebp_available(void);

/* Dimensioni di un'immagine WebP. Scrive w/h; 0 ok, -1 errore. */
int gmbwebp_info(const char* path, long* w, long* h);

/* WebP -> PPM (P6 RGB8). 0 ok, -1 errore. */
int gmbwebp_decode_ppm(const char* webp_path, const char* ppm_path);

/* PPM (P6 RGB8) -> WebP (lossy, quality 1..100). 0 ok, -1 errore. */
int gmbwebp_encode_ppm(const char* ppm_path, const char* webp_path, long quality);

/* Converte in base all'estensione di out (.ppm/.pgm = decode, altro = encode). 0 ok. */
int gmbwebp_convert(const char* in_path, const char* out_path, long quality);

/* --- API a buffer (usata dall'include nativo gmbmedia/mm_web, non dalla superficie BASIC) --- */
/* Decodifica WebP -> RGBA (malloc, liberare con gmbwebp_free). w/h via out. NULL err. */
unsigned char* gmbwebp_decode_rgba(const char* path, long* w, long* h);
/* Codifica RGBA -> WebP. 0 ok, -1 err. */
int gmbwebp_encode_rgba(const unsigned char* rgba, long w, long h, long quality, const char* out_path);
void gmbwebp_free(void* p);

#ifdef __cplusplus
}
#endif

#endif /* GMBWEBP_H */
