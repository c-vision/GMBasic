/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * gmbheif.h — API pubblica dello shim FFI del pacchetto `heif` (HEIC via libheif statica). */
#ifndef GMBHEIF_H
#define GMBHEIF_H

#ifdef __cplusplus
extern "C" {
#endif

int gmbheif_available(void);
int gmbheif_info(const char* path, long* w, long* h);
int gmbheif_decode_ppm(const char* heif_path, const char* ppm_path);
int gmbheif_encode_ppm(const char* ppm_path, const char* heif_path, long quality);
int gmbheif_convert(const char* in_path, const char* out_path, long quality);
unsigned char* gmbheif_decode_rgba(const char* path, long* w, long* h);
int gmbheif_encode_rgba(const unsigned char* rgba, long w, long h, long quality, const char* out_path);
void gmbheif_free(void* p);

#ifdef __cplusplus
}
#endif

#endif /* GMBHEIF_H */
