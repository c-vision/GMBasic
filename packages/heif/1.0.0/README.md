# heif — HEIC/HEIF (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbheif`). HEIC/HEIF **decode/encode** esposto a GMBasic via FFI
(namespace **HEIF**).

## Design (perché è self-contained)
Lo shim `libgmbheif.dylib` è costruito dal **package manager** (`build.sh`): **compila dai sorgenti**
`libde265` (HEVC decode), `x265` (HEVC encode) e `libheif` (container) e li **linka staticamente**
dentro lo shim — la libreria è **dentro il pacchetto**, **nessuna** lib di sistema a runtime.
Per il link statico dei "plugin" di libheif serve `-Wl,-force_load`.

## API (BASIC, namespace `HEIF`)
| Membro | Firma | Semantica |
|---|---|---|
| `HEIF.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `HEIF.INFO(path, w, h)` | `(STRING, BYREF LONG, BYREF LONG) AS LONG` | dimensioni; 0 ok |
| `HEIF.DECODEPPM(heic, ppm)` | `(STRING, STRING) AS LONG` | HEIC → PPM (P6 RGB8); 0 ok |
| `HEIF.ENCODEPPM(ppm, heic, q)` | `(STRING, STRING, LONG) AS LONG` | PPM → HEIC (q 1..100); 0 ok |
| `HEIF.CONVERT(in, out, q)` | `(STRING, STRING, LONG) AS LONG` | direzione da estensione di `out` |

## Build
Il PM esegue `build.sh` (richiede: `git`, `cmake`, compilatore). Compila 3 progetti dai sorgenti.

## Riferimenti
- `heif.inc`, `include/gmbheif.h`, `src/gmbheif.c`, `build.sh`. Sample: `samples/heif_demo.bas`.
