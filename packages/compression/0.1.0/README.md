# compression — pacchetto FFI (0.1.0)

Dominio di compressione richiamabile da BASIC tramite FFI. Sorgenti self-contained
nella cartella, copia canonica in `gmbsuite/gmbcompression`.

## Classificazione

È un **pacchetto** (non una `shared/libs`) perché usa librerie esterne: `zlib`,
`zstd`, `lz4`, `xz/lzma`, `bzip2`, `brotli`.

## Superficie (19 funzioni)

| funzione | input | output |
|---|---|---|
| `COMPRESS(data$, algo$)` | testo + algoritmo | base64 del flusso compresso |
| `DECOMPRESS(base64$, algo$)` | base64 + algoritmo | testo decompresso |
| `GZIP`/`UNGZIP` | testo / base64 | base64 / testo |
| `ZLIB`/`UNZLIB` | testo / base64 | base64 / testo |
| `DEFLATE`/`UNDEFLATE` | testo / base64 | base64 / testo |
| `BZIP2`/`UNBZIP2` | testo / base64 | base64 / testo |
| `BROTLI`/`UNBROTLI` | testo / base64 | base64 / testo |
| `ZSTD`/`UNZSTD` | testo / base64 | base64 / testo |
| `LZ4`/`UNLZ4` | testo / base64 | base64 / testo |
| `XZ`/`UNXZ` | testo / base64 | base64 / testo |
| `ALGORITHMS()` | — | `GZIP,DEFLATE,BROTLI,ZLIB,BZIP2,ZSTD,LZ4,XZ` |

**ZIP / UNZIP non sono nel pacchetto**: restano builtin del runtime (archivi STORE,
condivisi con package manager e brains). Una sola implementazione per funzione.

## Uso

```basic
REQUIRE "compression"
IMPORT COMPRESSION

z$ = GZIP("hello")
PRINT UNGZIP(z$)
```

## Build

```bash
bash build.sh        # produce lib/<platform>/libgmbcompression.dylib
```

Dipendenze: `brew install zstd lz4 xz brotli bzip2` (zlib di sistema).

## API C

`include/gmbcompression.h`:
- `ofx_cp_compress_b64(text, algo)`
- `ofx_cp_decompress_b64(b64, algo)`
- `ofx_cp_algorithms()`

Le stringhe di ritorno sono `char*` malloc'd: il bridge FFI le copia e libera.
