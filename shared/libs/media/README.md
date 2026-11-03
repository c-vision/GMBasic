# gmbmedia — media / audio-video

**Part of:** [gmbsuite](../README.md) · **Prefix:** `mm_` · **Dependencies:** C99 + libm
(advanced codec via **`dlsym`**, like PDFOxide in `gmb_doc`).

Media I/O and DSP. The **WAV PCM16/FLOAT32** core is self-contained; the codecs (ffmpeg) are optional at runtime.

## API
| Group | Functions |
|---|---|
| **WAV PCM int** | `mm_wav_info`, `mm_wav_read_pcm16`, `mm_wav_write_pcm16`/**`_pcm24`**/**`_pcm32`**, `mm_gain_pcm16` |
| **WAV float** | `mm_wav_read_float` (8/16/24/32/float32 → float [-1,1]), `mm_wav_write_float` (FLOAT32), `mm_audio_free` |
| **DSP (float)** | `mm_gain_float`, `mm_normalize_float`, `mm_fade_float`, `mm_reverse_float`, `mm_mix_float`, `mm_to_mono_float`, `mm_resample_linear`/`mm_resample_sinc`, `mm_biquad_lowpass`/`mm_biquad_float` |
| **Images** | `mm_image_read`/`mm_image_write` (by extension), **`mm_image_convert(src,dst)`**, **`mm_image_formats`**, `mm_image_free` |
| **PNG** | `mm_png_read` (8/16-bit gray/RGB/gray+alpha/RGBA/palette, non-interlaced or **Adam7**; 16→8 via high byte), `mm_png_write` — self-contained RFC1951 INFLATE |
| **JPEG** | `mm_jpeg_read` (baseline SOF0/1 + **progressive SOF2**: multiple scans, spectral selection Ss/Se, successive approximation Ah/Al, restart; 1/3 components), `mm_jpeg_write` (4:2:0, quality 90) / **`mm_jpeg_write_q`** (quality 1..100) / **`mm_jpeg_write_tables`** (custom base tables) — self-contained |
| **GIF** | `mm_gif_read` (first frame, LZW, global/local palette, interlace), `mm_gif_write` (3-3-2 + LZW) / **`mm_gif_write_mc`** (median-cut adaptive palette) |
| **BMP** | `mm_bmp_read`/`mm_bmp_write` (24/32-bit uncompressed) |
| **TGA** | `mm_tga_read`/`mm_tga_write` (truecolor 24/32, gray 8; uncompressed + RLE in read) |
| **TIFF** | `mm_tiff_read`/`mm_tiff_write` (8-bit gray/RGB/RGBA + **palette**/**CMYK** in read; little/big-endian; Compression none/PackBits, single/multiple strip; **multi-IFD** via `mm_tiff_read_page`/`mm_tiff_pages`) |
| **PNM** | `mm_pnm_read`/`mm_pnm_write` (P5 gray / P6 RGB) |
| **QOI** | `mm_qoi_read`/`mm_qoi_write` (Quite OK Image, lossless) |
| **WebP** | `mm_webp_read`/`mm_webp_write` — they load the **shim of the `webp` package** (self-contained, static libwebp) from the cache `~/.gmbasic/packages/webp/<ver>/lib/<platform>/`; **no system lib** |
| **Image ops** | `mm_img_resize_nn`/`_bilinear`, `mm_img_crop`, `mm_img_rotate90`, `mm_img_flip_h`/`_v`, `mm_img_to_gray`, `mm_img_to_channels`, `mm_img_invert`, `mm_img_brightness`, `mm_img_contrast`, `mm_img_threshold`, `mm_img_apply_lut`, `mm_img_blur_box`, `mm_img_blur_gaussian`, `mm_img_morph` (erode/dilate), `mm_img_dither`, `mm_img_warp_perspective`, `mm_img_sharpen`, `mm_img_sobel`, `mm_img_blend`, `mm_img_fill`, `mm_img_paste` |

## Test
```sh
make test     # 121 check: audio (PCM16/24/32, FLOAT32, DSP, biquad, sinc) + images (PNG/BMP/PNM/GIF/TGA/TIFF/JPEG/QOI/WebP)
              # + operations (resize/crop/rotate/flip/gray/invert/brightness/contrast/threshold/
              #   blur/sharpen/sobel/blend/fill) + conversion between formats (mm_image_convert)
```

## Roadmap
- Other containers (AIFF/FLAC/OGG) via dlsym; **video decode/encode**
  (ffmpeg `avcodec/avformat` via `dlsym`), transcode, mux/demux, frame extract.
- **Images**: **AVIF/HEIC** — via a dedicated package (self-contained shim, like `webp`); not with a system `dlsym`.
- Inspired by **ffmpeg/moviepy/opencv/librosa/Pillow**.

## References
- [[GMBSuite]] · `gmbgraphics` (images/3D). Inspired by **ffmpeg/moviepy/opencv/librosa**.
