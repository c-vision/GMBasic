# video — video reading (self-contained FFI shim)

gmbsuite package (C prefix: `gmbvideo`). Video reading (probe + frame extraction) exposed to GMBasic
via FFI (namespace **VIDEO**).

## Design (why it is self-contained)
The `libgmbvideo.dylib` shim is built by the **package manager** (`build.sh`): it **compiles ffmpeg
from source** (minimal build: mov/matroska/avi/mpegts demuxers…, h264/hevc/vp8/vp9/av1 decoders…,
swscale) and **links it statically** inside the shim. It depends only on **Apple system frameworks**
(CoreFoundation/CoreVideo/CoreMedia/VideoToolbox) and OS libs (zlib/bz2/iconv) — no third-party
library.

## API (BASIC, `VIDEO` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `VIDEO.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `VIDEO.INFO(path, w, h, dur)` | `(STRING, BYREF LONG, BYREF LONG, BYREF LONG) AS LONG` | dims + duration ms; 0 ok |
| `VIDEO.FRAME(path, t_ms, ppm)` | `(STRING, LONG, STRING) AS LONG` | frame at time t → PPM; 0 ok |
| `VIDEO.FIRSTFRAMEPPM(path, ppm)` | `(STRING, STRING) AS LONG` | first frame → PPM; 0 ok |

## Build
The package manager runs `build.sh` (requires: `git`, `make`, a C compiler). It compiles a minimal
ffmpeg from source.

## References
- `video.inc`, `include/gmbvideo.h`, `src/gmbvideo.c`, `build.sh`. Sample: `samples/video_demo.bas`.
