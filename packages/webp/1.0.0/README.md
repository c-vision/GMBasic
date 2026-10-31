# webp — WebP (self-contained FFI shim)

gmbsuite package (C prefix: `gmbwebp`). WebP **decode/encode** exposed to GMBasic via FFI
(namespace **WEBP**).

## Design (why it is self-contained)
The `libgmbwebp.dylib` shim is built by **linking `libwebp` statically** (+ `libsharpyuv`): the
package contains the library inside it, so **no system library** is required at runtime. The native
gmbsuite include `gmbmedia` (`mm_webp.c`) loads *this* shim from the package
(`~/.gmbasic/packages/webp/<ver>/lib/<platform>/`) — **never** a system lib. (Same model as the
`duckdb` package.)

## API (BASIC, `WEBP` namespace)
| Member | Signature | Semantics |
|---|---|---|
| `WEBP.AVAILABLE()` | `() AS LONG` | 1 = shim available |
| `WEBP.INFO(path, w, h)` | `(path AS STRING, BYREF w AS LONG, BYREF h AS LONG) AS LONG` | dimensions; 0 ok |
| `WEBP.DECODEPPM(webp, ppm)` | `(STRING, STRING) AS LONG` | WebP → PPM (P6 RGB8); 0 ok |
| `WEBP.ENCODEPPM(ppm, webp, q)` | `(STRING, STRING, LONG) AS LONG` | PPM → lossy WebP (q 1..100); 0 ok |
| `WEBP.CONVERT(in, out, q)` | `(STRING, STRING, LONG) AS LONG` | direction from the `out` extension; 0 ok |

## Usage
```basic
REQUIRE "webp"
PRINT WEBP.ENCODEPPM("in.ppm", "out.webp", 90)
PRINT WEBP.DECODEPPM("out.webp", "back.ppm")
```

## Build
```sh
bash build.sh     # requires: brew install webp (static libwebp + libsharpyuv)
```

## References
- `webp.inc` (FFI bindings), `include/gmbwebp.h`, `src/gmbwebp.c` (shim).
- Sample: `samples/webp_demo.bas`. Consuming native include: `gmbmedia` (`mm_webp.c`).
