/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * gmbavif.h — API pubblica dello shim FFI del pacchetto `avif` (AVIF via libavif statica). */
#ifndef GMBAVIF_H
#define GMBAVIF_H

#ifdef __cplusplus
extern "C" {
#endif

int gmbavif_available(void);
int gmbavif_info(const char* path, long* w, long* h);
int gmbavif_decode_ppm(const char* avif_path, const char* ppm_path);
int gmbavif_encode_ppm(const char* ppm_path, const char* avif_path, long quality);
int gmbavif_convert(const char* in_path, const char* out_path, long quality);
unsigned char* gmbavif_decode_rgba(const char* path, long* w, long* h);
int gmbavif_encode_rgba(const unsigned char* rgba, long w, long h, long quality, const char* out_path);
void gmbavif_free(void* p);

#ifdef __cplusplus
}
#endif

#endif /* GMBAVIF_H */
