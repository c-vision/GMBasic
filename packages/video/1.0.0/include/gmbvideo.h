/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * gmbvideo.h — API pubblica dello shim FFI del pacchetto `video` (ffmpeg statica). */
#ifndef GMBVIDEO_H
#define GMBVIDEO_H

#ifdef __cplusplus
extern "C" {
#endif

int gmbvideo_available(void);
/* Dimensioni + durata (ms) del primo stream video. 0 ok, -1 err. */
int gmbvideo_info(const char* path, long* w, long* h, long* dur_ms);
/* Estrae un frame al tempo t_ms in un file PPM (P6 RGB8). 0 ok, -1 err. */
int gmbvideo_frame(const char* path, long t_ms, const char* ppm_out);
/* Primo frame -> PPM. */
int gmbvideo_convert(const char* in_path, const char* ppm_out);

#ifdef __cplusplus
}
#endif

#endif /* GMBVIDEO_H */
