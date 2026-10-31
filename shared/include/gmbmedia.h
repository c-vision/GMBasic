/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gmbmedia.h — media (audio/video). PCM/WAV self-contained; codec avanzato via `dlsym` (roadmap). */
#ifndef GMBMEDIA_H
#define GMBMEDIA_H

#include <stddef.h>   /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { int channels, sample_rate, bits; long frames; double duration; } mm_wav_meta;

/* ── WAV PCM16 (interleaved) ── */
int mm_wav_info(const char* path, mm_wav_meta* out);
int mm_wav_read_pcm16(const char* path, short** pcm, long* frames, int* channels, int* sample_rate);
int mm_wav_write_pcm16(const char* path, const short* pcm, long frames, int channels, int sample_rate);
int mm_wav_write_pcm24(const char* path, const int* pcm, long frames, int channels, int sample_rate); /* int24 little-endian */
int mm_wav_write_pcm32(const char* path, const int* pcm, long frames, int channels, int sample_rate); /* int32 little-endian */
int mm_gain_pcm16(short* pcm, long samples, double gain_db);

/* ── audio float interleaved [-1,1] (qualsiasi bit depth in lettura) ── */
typedef struct { float* data; long frames; int channels, sample_rate; } mm_audio;
int  mm_wav_read_float(const char* path, mm_audio* out);    /* 8/16/24/32/float -> float; 0 ok */
int  mm_wav_write_float(const char* path, const mm_audio* a);/* scrive FLOAT32 WAV; 0 ok */
void mm_audio_free(mm_audio* a);

/* ── DSP (float) ── */
int mm_gain_float(float* x, long n, double gain_db);
int mm_normalize_float(float* x, long n);                   /* porta il picco a 1.0 */
int mm_fade_float(float* x, long frames, int channels, long fade_in, long fade_out);
int mm_reverse_float(float* x, long frames, int channels);
int mm_mix_float(float* dst, const float* a, const float* b, long n, double wa, double wb);
int mm_to_mono_float(const float* x, long frames, int channels, float* out);
int mm_resample_linear(const float* in, long in_frames, int ch, long out_frames, float* out);
int mm_resample_sinc(const float* in, long in_frames, int ch, long out_frames, float* out); /* sinc finestrato (Hann, anti-alias) */
void mm_biquad_lowpass(double fc, int sr, double q, double b[3], double a[3]); /* coeff (a[0]=1) */
int mm_biquad_float(float* x, long frames, int channels, const double b[3], const double a[3]); /* applica per canale */

/* ── immagini (raster, channels 1=grigio / 3=RGB / 4=RGBA) ── */
typedef struct { int width, height, channels; unsigned char* data; } mm_image;  /* row-major */
void mm_image_free(mm_image* img);

/* per estensione: .png/.gif/.jpg/.jpeg/.bmp/.tga/.ppm/.pgm */
int mm_image_read(const char* path, mm_image* out);
int mm_image_write(const char* path, const mm_image* img);
/* converte da un formato all'altro (quando possibile): legge `src`, scrive `dst` (per estensione). */
int mm_image_convert(const char* src, const char* dst);
/* Compressione adattiva (Luban-style): resize target + qualità (binary-search). quality>0 = fissa. */
int mm_image_compress(const char* src, const char* dst, int max_short, int quality);
int mm_jpeg_encode_mem(const mm_image* img, int quality, unsigned char** out, size_t* len);
/* elenco formati supportati per la lettura, separati da virgola (in `out`). */
void mm_image_formats(char* out, int cap);

int mm_bmp_read(const char* path, mm_image* out);       /* 24/32-bit non compresso */
int mm_bmp_write(const char* path, const mm_image* img);
int mm_pnm_read(const char* path, mm_image* out);       /* P5 (grigio) / P6 (RGB) */
int mm_pnm_write(const char* path, const mm_image* img);
int mm_png_read(const char* path, mm_image* out);       /* 8/16-bit gray/RGB/RGBA/palette, non interlacciato o Adam7 */
int mm_png_write(const char* path, const mm_image* img);
int mm_gif_read(const char* path, mm_image* out);       /* primo frame, LZW, palette globale/locale */
int mm_gif_write(const char* path, const mm_image* img);/* quantizzazione 3-3-2 + LZW */
int mm_gif_write_mc(const char* path, const mm_image* img, int maxcolors); /* palette adattiva median-cut */
int mm_jpeg_read(const char* path, mm_image* out);      /* baseline SOF0/SOF1 + progressivo SOF2, 1/3 componenti */
int mm_jpeg_read_scaled(const char* path, mm_image* out, int shrink); /* shrink-on-load (DCT scaling): 1,2,4,8 */
int mm_jpeg_write(const char* path, const mm_image* img);/* baseline 4:2:0, quality 90 */
int mm_jpeg_write_q(const char* path, const mm_image* img, int quality); /* quality 1..100 */
int mm_jpeg_write_tables(const char* path, const mm_image* img, const unsigned char lum[64], const unsigned char chr[64]); /* tabelle base (ordine naturale) */
int mm_qoi_read(const char* path, mm_image* out);       /* Quite OK Image, lossless */
int mm_qoi_write(const char* path, const mm_image* img);
int mm_tga_read(const char* path, mm_image* out);       /* truecolor/gray, non compresso o RLE */
int mm_tga_write(const char* path, const mm_image* img);/* truecolor 24/32 o gray 8, non compresso */
int mm_tiff_read(const char* path, mm_image* out);      /* TIFF: Compression none/PackBits, 8-bit gray/RGB/RGBA */
int mm_tiff_write(const char* path, const mm_image* img);/* TIFF: none o PackBits (scelta automatica), 8-bit */
int mm_tiff_read_page(const char* path, int page, mm_image* out); /* pagina N (multi-IFD) */
int mm_tiff_pages(const char* path);                    /* numero di IFD (pagine) */
int mm_webp_read(const char* path, mm_image* out);       /* WebP via dlsym (libwebp opzionale) -> RGBA */
int mm_webp_write(const char* path, const mm_image* img, int quality); /* WebP via dlsym (libwebp) */

/* ── operazioni immagine ── */
int mm_img_resize_nn(const mm_image* in, int w, int h, mm_image* out);
int mm_img_resize_bilinear(const mm_image* in, int w, int h, mm_image* out);
int mm_img_resize_lanczos(const mm_image* in, int w, int h, mm_image* out);
int mm_img_crop(const mm_image* in, int x, int y, int w, int h, mm_image* out);
int mm_img_rotate90(const mm_image* in, int turns, mm_image* out);
int mm_img_rotate_angle(const mm_image* in, double angle_deg, mm_image* out);
int mm_img_flip_h(const mm_image* in, mm_image* out);
int mm_img_flip_v(const mm_image* in, mm_image* out);
int mm_img_to_gray(const mm_image* in, mm_image* out);
int mm_img_to_channels(const mm_image* in, int channels, mm_image* out); /* 1/3/4 */
int mm_img_invert(mm_image* img);
int mm_img_brightness(mm_image* img, int delta);        /* somma (clamp) */
int mm_img_contrast(mm_image* img, double factor);      /* attorno a 128 */
int mm_img_threshold(mm_image* img, int t);             /* in b/n (soglia sul grigio) */
int mm_img_apply_lut(mm_image* img, const unsigned char lut[256]);
int mm_img_blur_box(const mm_image* in, int radius, mm_image* out);
int mm_img_blur_gaussian(const mm_image* in, int radius, mm_image* out); /* separabile, bordo replicato */
int mm_img_morph(const mm_image* in, int radius, int mode, mm_image* out); /* mode 0=erode, 1=dilate */
int mm_img_dither(const mm_image* in, int levels, mm_image* out);          /* Floyd-Steinberg a `levels` livelli/canale */
int mm_img_warp_perspective(const mm_image* in, const double m[9], int ow, int oh, mm_image* out); /* m: out->in (homografia 3x3) */
int mm_img_sharpen(const mm_image* in, mm_image* out);
int mm_img_sobel(const mm_image* in, mm_image* out);
int mm_img_blend(const mm_image* base, const mm_image* over, double alpha, mm_image* out);
int mm_img_fill(mm_image* img, int r, int g, int b, int a);
int mm_img_paste(mm_image* dst, const mm_image* src, int x, int y);

#ifdef __cplusplus
}
#endif
#endif /* GMBMEDIA_H */
