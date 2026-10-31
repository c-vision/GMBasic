/* SPDX-License-Identifier: LicenseRef-PUL-1.1
 * Copyright (c) 2026 Gaetano Minardi. Tutti i diritti riservati.
 * Licenza: Personal Use License (PUL) v1.1.
 *
 * gmbmedia_basic.h — superficie BASIC-friendly (namespace MEDIA) di gmbmedia.
 * Handle LONG, scalari/stringhe: adatta a FFI (DECLARE ... LIB "gmbmedia") e al link statico.
 * Direzione unica: il consumatore (BASIC/C#/compilatore) chiama; la logica sta in gmbmedia. */
#ifndef GMBMEDIA_BASIC_H
#define GMBMEDIA_BASIC_H

#ifdef __cplusplus
extern "C" {
#endif

int  gmbmedia_available(void);

/* immagini (handle LONG; -1 su errore) */
long gmbmedia_img_read(const char* path);
long gmbmedia_img_read_scaled(const char* path, long shrink); /* JPEG shrink-on-load: 1,2,4,8 */
long gmbmedia_img_w(long h);
long gmbmedia_img_h(long h);
long gmbmedia_img_channels(long h);
long gmbmedia_img_get(long h, long x, long y);       /* 0xRRGGBB, -1 fuori range */
long gmbmedia_img_get_a(long h, long x, long y);     /* alpha 0..255 */
long gmbmedia_img_set(long h, long x, long y, long rgb, long a);
long gmbmedia_img_write(long h, const char* path);
long gmbmedia_img_write_q(long h, const char* path, long quality);
long gmbmedia_img_convert(const char* src, const char* dst);
long gmbmedia_img_compress(const char* src, const char* dst, long max_short, long quality);
const char* gmbmedia_img_formats(void);
long gmbmedia_img_free(long h);

long gmbmedia_img_resize(long h, long w, long hh, long mode);   /* mode 0=nn 1=bilinear */
long gmbmedia_img_crop(long h, long x, long y, long w, long hh);
long gmbmedia_img_rotate(long h, long turns);
long gmbmedia_img_rotate_angle(long h, long angle_cdeg);
long gmbmedia_img_flip(long h, long axis);                      /* 0=h 1=v */
long gmbmedia_img_gray(long h);
long gmbmedia_img_to_channels(long h, long ch);
long gmbmedia_img_blur(long h, long radius, long kind);          /* kind 0=box 1=gaussian */
long gmbmedia_img_morph(long h, long radius, long mode);         /* 0=erode 1=dilate */
long gmbmedia_img_dither(long h, long levels);
long gmbmedia_img_sharpen(long h);
long gmbmedia_img_sobel(long h);
long gmbmedia_img_blend(long base, long over, long alpha_pct);
long gmbmedia_img_warp(long h, const char* m_csv, long ow, long oh);

long gmbmedia_img_invert(long h);
long gmbmedia_img_brightness(long h, long delta);
long gmbmedia_img_contrast(long h, long factor_pct);
long gmbmedia_img_threshold(long h, long t);
long gmbmedia_img_fill(long h, long rgb, long a);
long gmbmedia_img_paste(long dst, long src, long x, long y);

/* audio (float interleaved) */
long gmbmedia_audio_read(const char* path);
long gmbmedia_audio_frames(long h);
long gmbmedia_audio_channels(long h);
long gmbmedia_audio_rate(long h);
long gmbmedia_audio_sample(long h, long frame, long ch);        /* int16 con segno */
long gmbmedia_audio_set(long h, long frame, long ch, long val16);
long gmbmedia_audio_write(long h, const char* path);
long gmbmedia_audio_free(long h);
long gmbmedia_audio_gain(long h, long gain_cdb);                /* centesimi di dB */
long gmbmedia_audio_normalize(long h);
long gmbmedia_audio_fade(long h, long fade_in, long fade_out);
long gmbmedia_audio_reverse(long h);
long gmbmedia_audio_to_mono(long h);
long gmbmedia_audio_resample(long h, long out_frames, long kind); /* kind 0=linear 1=sinc */
long gmbmedia_audio_mix(long a_h, long b_h, long wa_pct, long wb_pct);

long gmbmedia_wav_channels(const char* path);
long gmbmedia_wav_rate(const char* path);
long gmbmedia_wav_bits(const char* path);
long gmbmedia_wav_frames(const char* path);

#ifdef __cplusplus
}
#endif
#endif /* GMBMEDIA_BASIC_H */
