# video — lettura video (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbvideo`). Lettura video (probe + estrazione frame) esposta a
GMBasic via FFI (namespace **VIDEO**).

## Design (perché è self-contained)
Lo shim `libgmbvideo.dylib` è costruito dal **package manager** (`build.sh`): **compila ffmpeg
dai sorgenti** (build minimale: demuxer mov/matroska/avi/mpegts…, decoder h264/hevc/vp8/vp9/av1…,
swscale) e lo **linka staticamente** dentro lo shim. Dipende solo da framework **di sistema Apple**
(CoreFoundation/CoreVideo/CoreMedia/VideoToolbox) e libs OS (zlib/bz2/iconv) — nessuna lib terza-party.

## API (BASIC, namespace `VIDEO`)
| Membro | Firma | Semantica |
|---|---|---|
| `VIDEO.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `VIDEO.INFO(path, w, h, dur)` | `(STRING, BYREF LONG, BYREF LONG, BYREF LONG) AS LONG` | dims + durata ms; 0 ok |
| `VIDEO.FRAME(path, t_ms, ppm)` | `(STRING, LONG, STRING) AS LONG` | frame al tempo t → PPM; 0 ok |
| `VIDEO.FIRSTFRAMEPPM(path, ppm)` | `(STRING, STRING) AS LONG` | primo frame → PPM; 0 ok |

## Build
Il PM esegue `build.sh` (richiede: `git`, `make`, compilatore). Compila ffmpeg dai sorgenti (minimale).

## Riferimenti
- `video.inc`, `include/gmbvideo.h`, `src/gmbvideo.c`, `build.sh`. Sample: `samples/video_demo.bas`.
