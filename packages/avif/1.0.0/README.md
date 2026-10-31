# avif — AVIF (self-contained FFI shim)

gmbsuite package (C prefix: `gmbavif`). AVIF **decode/encode** exposed to GMBasic via FFI
(namespace **AVIF**).

## Design (why it is self-contained)
The `libgmbavif.dylib` shim is built by the **package manager** (`build.sh`): it **compiles libavif
from source** (git) and **links it statically** together with `libaom.a` + `libvmaf.a` (static) —
the library is **inside the package**, **no** system dependency at runtime. (Same model as
`duckdb`/`webp`; here the codec is compiled from source because libavif ships no prebuilt static
libs.) AV1 via **aom** (dav1d is not needed).

## API (BASIC, `AVIF` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `AVIF.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `AVIF.INFO(path, w, h)` | `(STRING, BYREF LONG, BYREF LONG) AS LONG` | dimensions; 0 ok |
| `AVIF.DECODEPPM(avif, ppm)` | `(STRING, STRING) AS LONG` | AVIF → PPM (P6 RGB8); 0 ok |
| `AVIF.ENCODEPPM(ppm, avif, q)` | `(STRING, STRING, LONG) AS LONG` | PPM → AVIF (q 1..100); 0 ok |
| `AVIF.CONVERT(in, out, q)` | `(STRING, STRING, LONG) AS LONG` | direction from the `out` extension |

## Build
The package manager runs `build.sh` (requires: `git`, `cmake`, `brew aom libvmaf` for the static
libs).

## References
- `avif.inc`, `include/gmbavif.h`, `src/gmbavif.c`, `build.sh`. Sample: `samples/avif_demo.bas`.
