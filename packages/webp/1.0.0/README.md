# webp — WebP (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbwebp`). WebP **decode/encode** esposto a GMBasic via FFI
(namespace **WEBP**).

## Design (perché è self-contained)
Lo shim `libgmbwebp.dylib` viene costruito **linkando staticamente** `libwebp` (+ `libsharpyuv`):
il pacchetto contiene la libreria al suo interno, quindi **nessuna libreria di sistema** è
richiesta a runtime. L'include nativo di gmbsuite `gmbmedia` (`mm_webp.c`) carica *questo* shim
dal pacchetto (`~/.gmb/packages/webp/<ver>/lib/<platform>/`) — **mai** una lib di sistema.
(Stesso modello del pacchetto `duckdb`.)

## API (BASIC, namespace `WEBP`)
| Membro | Firma | Semantica |
|---|---|---|
| `WEBP.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `WEBP.INFO(path, w, h)` | `(path AS STRING, BYREF w AS LONG, BYREF h AS LONG) AS LONG` | dimensioni; 0 ok |
| `WEBP.DECODEPPM(webp, ppm)` | `(STRING, STRING) AS LONG` | WebP → PPM (P6 RGB8); 0 ok |
| `WEBP.ENCODEPPM(ppm, webp, q)` | `(STRING, STRING, LONG) AS LONG` | PPM → WebP lossy (q 1..100); 0 ok |
| `WEBP.CONVERT(in, out, q)` | `(STRING, STRING, LONG) AS LONG` | direzione da estensione di `out`; 0 ok |

## Uso
```basic
REQUIRE "webp"
PRINT WEBP.ENCODEPPM("in.ppm", "out.webp", 90)
PRINT WEBP.DECODEPPM("out.webp", "back.ppm")
```

## Build
```sh
bash build.sh     # richiede: brew install webp (libwebp + libsharpyuv statiche)
```

## Riferimenti
- `webp.inc` (binding FFI), `include/gmbwebp.h`, `src/gmbwebp.c` (shim).
- Sample: `samples/webp_demo.bas`. Include nativo consumatore: `gmbmedia` (`mm_webp.c`).
