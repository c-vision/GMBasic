# heif — HEIC/HEIF (self-contained FFI shim)

gmbsuite package (C prefix: `gmbheif`). HEIC/HEIF **decode/encode** exposed to GMBasic via FFI
(namespace **HEIF**).

## Design (why it is self-contained)
The `libgmbheif.dylib` shim is built by the **package manager** (`build.sh`): it **compiles from
source** `libde265` (HEVC decode), `x265` (HEVC encode) and `libheif` (container) and **links them
statically** inside the shim — the library is **inside the package**, **no** system lib at runtime.
Static linking of the libheif "plugins" needs `-Wl,-force_load`.

## API (BASIC, `HEIF` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `HEIF.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `HEIF.INFO(path, w, h)` | `(STRING, BYREF LONG, BYREF LONG) AS LONG` | dimensions; 0 ok |
| `HEIF.DECODEPPM(heic, ppm)` | `(STRING, STRING) AS LONG` | HEIC → PPM (P6 RGB8); 0 ok |
| `HEIF.ENCODEPPM(ppm, heic, q)` | `(STRING, STRING, LONG) AS LONG` | PPM → HEIC (q 1..100); 0 ok |
| `HEIF.CONVERT(in, out, q)` | `(STRING, STRING, LONG) AS LONG` | direction from the `out` extension |

## Build
The package manager runs `build.sh` (requires: `git`, `cmake`, a C compiler). It compiles 3 projects
from source.

## References
- `heif.inc`, `include/gmbheif.h`, `src/gmbheif.c`, `build.sh`. Sample: `samples/heif_demo.bas`.
