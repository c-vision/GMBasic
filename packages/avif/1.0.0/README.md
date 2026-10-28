# avif — AVIF (shim FFI self-contained)

Pacchetto gmbsuite (Prefisso C: `gmbavif`). AVIF **decode/encode** esposto a GMBasic via FFI
(namespace **AVIF**).

## Design (perché è self-contained)
Lo shim `libgmbavif.dylib` viene costruito dal **package manager** (`build.sh`): **compila
libavif dai sorgenti** (git) e la **linka staticamente** insieme a `libaom.a` + `libvmaf.a`
(statiche) — la libreria è **dentro il pacchetto**, **nessuna** dipendenza di sistema a runtime.
(Stesso modello di `duckdb`/`webp`; qui il codec è compilato dai sorgenti perché non esistono
statiche preconfezionate di libavif.) AV1 via **aom** (non serve davari1d).

## API (BASIC, namespace `AVIF`)
| Membro | Firma | Semantica |
|---|---|---|
| `AVIF.AVAILABLE()` | `() AS LONG` | 1 = shim disponibile |
| `AVIF.INFO(path, w, h)` | `(STRING, BYREF LONG, BYREF LONG) AS LONG` | dimensioni; 0 ok |
| `AVIF.DECODEPPM(avif, ppm)` | `(STRING, STRING) AS LONG` | AVIF → PPM (P6 RGB8); 0 ok |
| `AVIF.ENCODEPPM(ppm, avif, q)` | `(STRING, STRING, LONG) AS LONG` | PPM → AVIF (q 1..100); 0 ok |
| `AVIF.CONVERT(in, out, q)` | `(STRING, STRING, LONG) AS LONG` | direzione da estensione di `out` |

## Build
Il PM esegue `build.sh` (richiede: `git`, `cmake`, `brew aom libvmaf` per le statiche).

## Riferimenti
- `avif.inc`, `include/gmbavif.h`, `src/gmbavif.c`, `build.sh`. Sample: `samples/avif_demo.bas`.
