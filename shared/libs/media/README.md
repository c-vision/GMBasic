# gmbmedia — media / audio-video

**Parte di:** [gmbsuite](../README.md) · **Prefisso:** `mm_` · **Dipendenze:** C99 + libm
(codec avanzato via **`dlsym`**, come PDFOxide in `gmb_doc`).

Media I/O e DSP. Il core **WAV PCM16/FLOAT32** è self-contained; i codec (ffmpeg) sono opzionali a runtime.

## API
| Gruppo | Funzioni |
|---|---|
| **WAV PCM int** | `mm_wav_info`, `mm_wav_read_pcm16`, `mm_wav_write_pcm16`/**`_pcm24`**/**`_pcm32`**, `mm_gain_pcm16` |
| **WAV float** | `mm_wav_read_float` (8/16/24/32/float32 → float [-1,1]), `mm_wav_write_float` (FLOAT32), `mm_audio_free` |
| **DSP (float)** | `mm_gain_float`, `mm_normalize_float`, `mm_fade_float`, `mm_reverse_float`, `mm_mix_float`, `mm_to_mono_float`, `mm_resample_linear`/`mm_resample_sinc`, `mm_biquad_lowpass`/`mm_biquad_float` |
| **Immagini** | `mm_image_read`/`mm_image_write` (per estensione), **`mm_image_convert(src,dst)`**, **`mm_image_formats`**, `mm_image_free` |
| **PNG** | `mm_png_read` (8/16-bit gray/RGB/gray+alpha/RGBA/palette, non interlacciato o **Adam7**; 16→8 via byte alto), `mm_png_write` — INFLATE RFC1951 self-contained |
| **JPEG** | `mm_jpeg_read` (baseline SOF0/1 + **progressivo SOF2**: scan multipli, selezione spettrale Ss/Se, approssimazione successiva Ah/Al, restart; 1/3 componenti), `mm_jpeg_write` (4:2:0, quality 90) / **`mm_jpeg_write_q`** (quality 1..100) / **`mm_jpeg_write_tables`** (tabelle base custom) — self-contained |
| **GIF** | `mm_gif_read` (primo frame, LZW, palette globale/locale, interlace), `mm_gif_write` (3-3-2 + LZW) / **`mm_gif_write_mc`** (palette adattiva median-cut) |
| **BMP** | `mm_bmp_read`/`mm_bmp_write` (24/32-bit non compresso) |
| **TGA** | `mm_tga_read`/`mm_tga_write` (truecolor 24/32, gray 8; non compresso + RLE in lettura) |
| **TIFF** | `mm_tiff_read`/`mm_tiff_write` (8-bit grigio/RGB/RGBA + **palette**/**CMYK** in lettura; little/big-endian; Compression none/PackBits, strip singolo/multiplo; **multi-IFD** via `mm_tiff_read_page`/`mm_tiff_pages`) |
| **PNM** | `mm_pnm_read`/`mm_pnm_write` (P5 grigio / P6 RGB) |
| **QOI** | `mm_qoi_read`/`mm_qoi_write` (Quite OK Image, lossless) |
| **WebP** | `mm_webp_read`/`mm_webp_write` — caricano lo **shim del pacchetto `webp`** (self-contained, libwebp statica) dalla cache `~/.gmb/packages/webp/<ver>/lib/<platform>/`; **nessuna lib di sistema** |
| **Op. immagine** | `mm_img_resize_nn`/`_bilinear`, `mm_img_crop`, `mm_img_rotate90`, `mm_img_flip_h`/`_v`, `mm_img_to_gray`, `mm_img_to_channels`, `mm_img_invert`, `mm_img_brightness`, `mm_img_contrast`, `mm_img_threshold`, `mm_img_apply_lut`, `mm_img_blur_box`, `mm_img_blur_gaussian`, `mm_img_morph` (erode/dilate), `mm_img_dither`, `mm_img_warp_perspective`, `mm_img_sharpen`, `mm_img_sobel`, `mm_img_blend`, `mm_img_fill`, `mm_img_paste` |

## Test
```sh
make test     # 121 check: audio (PCM16/24/32, FLOAT32, DSP, biquad, sinc) + immagini (PNG/BMP/PNM/GIF/TGA/TIFF/JPEG/QOI/WebP)
              # + operazioni (resize/crop/rotate/flip/gray/invert/brightness/contrast/threshold/
              #   blur/sharpen/sobel/blend/fill) + conversione tra formati (mm_image_convert)
```

## Roadmap
- Altri container (AIFF/FLAC/OGG) via dlsym; **decode/encode video**
  (ffmpeg `avcodec/avformat` via `dlsym`), transcode, mux/demux, frame extract.
- **Immagini**: **AVIF/HEIC** — via un pacchetto dedicato (shim self-contained, come `webp`); non con `dlsym` di sistema.
- Ispirato a **ffmpeg/moviepy/opencv/librosa/Pillow**.

## Riferimenti
- [[GMBSuite]] · `gmbgraphics` (immagini/3D). Ispirato a **ffmpeg/moviepy/opencv/librosa**.
