# V.30 — Compressione e dati binari

## Cosa ottieni da questo capitolo

Comprimere e decomprimere in memoria e su file, con l'algoritmo giusto per il lavoro giusto. È un
capitolo corto perché l'API è corta: la difficoltà è nella scelta, non nella chiamata.

## 1. Il giro completo

```
algoritmi: GZIP,DEFLATE,BROTLI,ZLIB,BZIP2,ZSTD,LZ4,XZ
gzip:    The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.
brotli:  The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.
zlib:    The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.
bzip2:   The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.
deflate: The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.
generico:The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.
zip:     contenuto archivio
```

Eseguito (`shared/samples/compression/CompressionRoundTrip.bas`, backend C99Native). Il test è
quello giusto: comprimi, decomprimi, **confronta con l'originale**. Round-trip identico su tutti gli
algoritmi; `generico` è la variante che non nomina l'algoritmo (usa il default).

## 2. Quale algoritmo

| Algoritmo | Rapporto | Velocità | Quando |
|---|---|---|---|
| `LZ4` | basso | altissima | dati temporanei, cache, log |
| `ZSTD` | alto | alta | scelta predefinita moderna |
| `GZIP`/`DEFLATE` | medio | media | interoperabilità, HTTP |
| `ZLIB` | medio | media | formati embedded (PNG, PDF) |
| `BROTLI` | molto alto | lenta | contenuti statici serviti molte volte |
| `BZIP2` | alto | molto lenta | archivi storici |
| `XZ` | molto alto | lenta | distribuzione di pacchetti |

La regola pratica: se comprimi e decomprimi una volta sola, la lentezza conta poco e vince il
rapporto; se decomprimi a ogni richiesta, vince la velocità.

## 3. In memoria e su file

- compressione **in memoria**: una stringa o un array di byte entra, uno più piccolo esce — è la
  forma da usare per un payload di rete;
- compressione **su file**: si legge il file, si scrive l'archivio;
- archivi **zip** con più voci: `compression/ZipUnzipDemo.bas` (estrazione compresa);
- il sample `CompressionAlgorithms.bas` confronta i formati, `CompressionFullCoverage.bas` copre
  le funzioni una per una.

## 4. Dati binari

Comprimere serve spesso perché il dato **è** binario: un array di byte, un record a lunghezza
fissa, un'immagine. Le funzioni di conversione (`HEX`, `UNHEX`, array di byte) e le operazioni
bit a bit di `BITS` sono il livello sotto. Il capitolo IV.22 spiega come si scrive un array su file.

## 5. Quando non comprimere

- dati già compressi (immagini JPEG, video, archivi): il guadagno è nullo e il costo è reale;
- payload molto piccoli (meno di ~100 byte): l'header pesa più del contenuto;
- dati che devono essere letti a caso dentro il file: comprimere richiede di decomprimere tutto;
- dati che devono essere confrontabili byte a byte.

## 6. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| il round-trip non torna | buffer di uscita troppo piccolo | dimensiona sull'originale + margine |
| archivio illeggibile da altri tool | formato proprietario | usa gzip/zip standard |
| nessun guadagno | dato già compresso | non comprimere |
| memoria esplosa | compressione a blocchi enormi | comprimi a blocchi |
| errore su file grande | letto tutto in RAM | usa l'API su file |

## 7. Dove andare adesso

- **IV.22** — file, record e binario.
- **IV.24** — compressione dei payload HTTP.
- **V.31** — i tensori, dove il binario diventa pesante.













<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **BITS.BITCLEAR(x1, x2)** — `RETURN BITS.BITCLEAR(perm, bit)` (shared/samples/Bits/BitsFullCoverage.bas:32)
- **BITS.BITCOUNT(x1)** — `PRINT "accesi="; BITS.BITCOUNT(perm)            ' 2` (shared/samples/Bits/BitsFullCoverage.bas:62)
- **BITS.BITDEINTERLEAVE(x1, x2)** — `IF BITS.BITDEINTERLEAVE(morton, 0) <> gx THEN morton_bad = morton_bad + 1` (shared/samples/Bits/BitsFullCoverage.bas:124)
- **BITS.BITFIELD(x1, x2, x3)** — `RETURN BITS.BITFIELD(h, lo, hi)` (shared/samples/Bits/BitsFullCoverage.bas:42)
- **BITS.BITGET(x1, x2)** — `PRINT "bit3="; BITS.BITGET(perm, 3)             ' 0` (shared/samples/Bits/BitsFullCoverage.bas:55)
- **BITS.BITINTERLEAVE(x1, x2)** — `morton = BITS.BITINTERLEAVE(gx, gy)` (shared/samples/Bits/BitsFullCoverage.bas:123)
- **BITS.BITREVERSE(x1, x2)** — `cipher = BITS.BITROTATEL(BITS.BITREVERSE(clear, 8), 3, 8)` (shared/samples/Bits/BitsFullCoverage.bas:137)
- **BITS.BITROTATEL(x1, x2, x3)** — `cipher = BITS.BITROTATEL(BITS.BITREVERSE(clear, 8), 3, 8)` (shared/samples/Bits/BitsFullCoverage.bas:137)
- **BITS.BITROTATER(x1, x2, x3)** — `back = BITS.BITREVERSE(BITS.BITROTATER(cipher, 3, 8), 8)` (shared/samples/Bits/BitsFullCoverage.bas:139)
- **BITS.BITSCANFWD(x1)** — `PRINT "lsb="; BITS.BITSCANFWD(40)                ' 3 (40 = 0b101000)` (shared/samples/Bits/BitsFullCoverage.bas:155)
- **BITS.BITSCANREV(x1)** — `PRINT "msb="; BITS.BITSCANREV(40)                ' 5` (shared/samples/Bits/BitsFullCoverage.bas:156)
- **BITS.BITSET(x1, x2)** — `RETURN BITS.BITSET(perm, bit)` (shared/samples/Bits/BitsFullCoverage.bas:30)
- **BITS.BITTEST(x1, x2)** — `RETURN BITS.BITTEST(perm, bit)` (shared/samples/Bits/BitsFullCoverage.bas:24)
- **BITS.BITTOGGLE(x1, x2)** — `perm = BITS.BITTOGGLE(perm, 2)                  ' revoca ESECUZIONE` (shared/samples/Bits/BitsFullCoverage.bas:56)
- **BITS.BYTESWAP(x1)** — `PRINT "byteswap2="; BITS.BYTESWAP(BITS.BYTESWAP(12345))   ' 12345` (shared/samples/Bits/BitsFullCoverage.bas:144)
- **BITS.FROMGRAY(x1)** — `IF BITS.FROMGRAY(BITS.GRAYCODE(k)) <> k THEN viol = viol + 1` (shared/samples/Bits/BitsFullCoverage.bas:108)
- **BITS.GRAYCODE(x1)** — `IF BITS.FROMGRAY(BITS.GRAYCODE(k)) <> k THEN viol = viol + 1` (shared/samples/Bits/BitsFullCoverage.bas:108)
- **BITS.HAMMINGDIST(x1, x2)** — `hamsum = hamsum + BITS.HAMMINGDIST(headers(k), headers(k + 1))` (shared/samples/Bits/BitsFullCoverage.bas:97)
- **BITS.ISPOW2(x1)** — `PRINT "cap_ispow="; BITS.ISPOW2(cap)             ' -1` (shared/samples/Bits/BitsFullCoverage.bas:153)
- **BITS.NEXTPOW2(x1)** — `cap = BITS.NEXTPOW2(need)` (shared/samples/Bits/BitsFullCoverage.bas:151)
- **BITS.PARITY(x1)** — `par = par + BITS.PARITY(headers(k))` (shared/samples/Bits/BitsFullCoverage.bas:96)
- **COMPRESSION.ALGORITHMS()** — `PRINT "Algoritmi supportati: "; COMPRESSION.ALGORITHMS()` (shared/samples/compression/CompressionBasics.bas:49)
- **COMPRESSION.BROTLI(x1)** — `DIM br$ = COMPRESSION.BROTLI(testo$)` (shared/samples/compression/CompressionBasics.bas:26)
- **COMPRESSION.BZIP2(x1)** — `DIM bz$ = COMPRESSION.BZIP2(testo$)` (shared/samples/compression/CompressionBasics.bas:38)
- **COMPRESSION.COMPRESS(x1, x2)** — `DIM c1$ = COMPRESSION.COMPRESS(testo$, "GZIP")` (shared/samples/compression/CompressionBasics.bas:44)
- **COMPRESSION.DECOMPRESS(x1, x2)** — `PRINT "COMPRESS(.., 'GZIP') roundtrip: "; COMPRESSION.DECOMPRESS(c1$, "GZIP") = testo$` (shared/samples/compression/CompressionBasics.bas:45)
- **COMPRESSION.DEFLATE(x1)** — `DIM df$ = COMPRESSION.DEFLATE(testo$)` (shared/samples/compression/CompressionBasics.bas:20)
- **COMPRESSION.GZIP(x1)** — `DIM gz$ = COMPRESSION.GZIP(testo$)` (shared/samples/compression/CompressionBasics.bas:14)
- **COMPRESSION.LZ4(x1)** — `PRINT "LZ4   compresso: "; LEN(C.LZ4(testo)); " byte  round-trip: "; C.UNLZ4(C.LZ4(testo)) = testo` (shared/samples/compression/CompressionAlgorithms.bas:16)
- **COMPRESSION.UNBROTLI(x1)** — `PRINT "  roundtrip: "; COMPRESSION.UNBROTLI(br$) = testo$` (shared/samples/compression/CompressionBasics.bas:28)
- **COMPRESSION.UNBZIP2(x1)** — `PRINT "  roundtrip: "; COMPRESSION.UNBZIP2(bz$) = testo$` (shared/samples/compression/CompressionBasics.bas:40)
- **COMPRESSION.UNDEFLATE(x1)** — `PRINT "  roundtrip: "; COMPRESSION.UNDEFLATE(df$) = testo$` (shared/samples/compression/CompressionBasics.bas:22)
- **COMPRESSION.UNGZIP(x1)** — `PRINT "  roundtrip: "; COMPRESSION.UNGZIP(gz$) = testo$` (shared/samples/compression/CompressionBasics.bas:16)
- **COMPRESSION.UNLZ4(x1)** — `PRINT "LZ4   compresso: "; LEN(C.LZ4(testo)); " byte  round-trip: "; C.UNLZ4(C.LZ4(testo)) = testo` (shared/samples/compression/CompressionAlgorithms.bas:16)
- **COMPRESSION.UNXZ(x1)** — `PRINT "XZ    compresso: "; LEN(C.XZ(testo)); " byte  round-trip: "; C.UNXZ(C.XZ(testo)) = testo` (shared/samples/compression/CompressionAlgorithms.bas:17)
- **COMPRESSION.UNZIP(x1, x2)** — `COMPRESSION.UNZIP(tmpZip, staging)` (shared/samples/PackageRegistry/PackageServer.bas:1789)
- **COMPRESSION.UNZLIB(x1)** — `PRINT "  roundtrip: "; COMPRESSION.UNZLIB(zl$) = testo$` (shared/samples/compression/CompressionBasics.bas:34)
- **COMPRESSION.UNZSTD(x1)** — `PRINT "ZSTD  compresso: "; LEN(z); " byte  round-trip: "; C.UNZSTD(z) = testo` (shared/samples/compression/CompressionAlgorithms.bas:15)
- **COMPRESSION.XZ(x1)** — `PRINT "XZ    compresso: "; LEN(C.XZ(testo)); " byte  round-trip: "; C.UNXZ(C.XZ(testo)) = testo` (shared/samples/compression/CompressionAlgorithms.bas:17)
- **COMPRESSION.ZIP(x1, x2)** — `ok = COMPRESSION.ZIP(srcs, tmp)` (shared/samples/PackageRegistry/PackageServer.bas:487)
- **COMPRESSION.ZLIB(x1)** — `DIM zl$ = COMPRESSION.ZLIB(testo$)` (shared/samples/compression/CompressionBasics.bas:32)
- **COMPRESSION.ZSTD(x1)** — `z = C.ZSTD(testo)` (shared/samples/compression/CompressionAlgorithms.bas:14)
<!-- END DOMAIN TABLE -->

<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### COMPRESSION-Guida-Utente

*Fonte: `COMPRESSION-Guida-Utente`*


## 1. Funzioni (namespace `COMPRESSION`)

### Generiche
| funzione | parametri | ritorno |
|---|---|---|
| `COMPRESSION.COMPRESS(data$, algorithm$)` | `data$` testo, `algorithm$` nome algoritmo | STRING (Base64) |
| `COMPRESSION.DECOMPRESS(data$, algorithm$)` | `data$` Base64, `algorithm$` nome | STRING (testo) |
| `COMPRESSION.ALGORITHMS()` | — | STRING (elenco separato da virgole) |

### Coppie dedicate (comprimi/decomprimi)
| comprimi | decomprimi | libreria |
|---|---|---|
| `COMPRESSION.GZIP(data$)` | `COMPRESSION.UNGZIP(data$)` | zlib |
| `COMPRESSION.ZLIB(data$)` | `COMPRESSION.UNZLIB(data$)` | zlib |
| `COMPRESSION.DEFLATE(data$)` | `COMPRESSION.UNDEFLATE(data$)` | zlib |
| `COMPRESSION.BZIP2(data$)` | `COMPRESSION.UNBZIP2(data$)` | libbz2 |
| `COMPRESSION.BROTLI(data$)` | `COMPRESSION.UNBROTLI(data$)` | brotli |
| **`COMPRESSION.ZSTD(data$)`** | **`COMPRESSION.UNZSTD(data$)`** | libzstd |
| **`COMPRESSION.LZ4(data$)`** | **`COMPRESSION.UNLZ4(data$)`** | liblz4 |
| **`COMPRESSION.XZ(data$)`** | **`COMPRESSION.UNXZ(data$)`** | liblzma |

### Archivi ZIP
| comando | parametri | effetto |
|---|---|---|
| `COMPRESSION.ZIP(sources$, dest$)` | `sources$` percorsi (`;` o array), `dest$` | crea archivio ZIP (STORE) → BIT |
| `COMPRESSION.UNZIP(archive$, dir$)` | `archive$`, `dir$` destinazione | estrae → BIT |

### Parametri (dettaglio)

### Esempio

IMPORT COMPRESSION AS C

SUB Main()
    DIM s AS STRING
    s = "The quick brown fox..."

    ' generico
    DIM z AS STRING
    z = C.COMPRESS(s, "zstd")
    PRINT "round-trip: "; C.DECOMPRESS(z, "zstd") = s

    ' dedicato
    PRINT "zstd: "; C.UNZSTD(C.ZSTD(s)) = s
    PRINT "lz4:  "; C.UNLZ4(C.LZ4(s)) = s
    PRINT "xz:   "; C.UNXZ(C.XZ(s)) = s



### Polimorfismo / console-aware

## 2. Note implementative

## 3. Riferimenti

### Compression-Guida-Utente

*Fonte: `Compression-Guida-Utente`*


## 1. Panoramica

IMPORT COMPRESSION AS C
SUB Main()
    PRINT C.UNGZIP(C.GZIP("hello"))
END SUB

| Funzione | Firma | Descrizione |
|---|---|---|
| `ALGORITHMS` | `ALGORITHMS()` → STRING | elenco algoritmi: `GZIP,DEFLATE,BROTLI,ZLIB,BZIP2` |
| `GZIP` / `UNGZIP` | `(data$)` → STRING | compress/decompress gzip |
| `DEFLATE` / `UNDEFLATE` | `(data$)` → STRING | deflate raw |
| `BROTLI` / `UNBROTLI` | `(data$)` → STRING | brotli |
| `ZLIB` / `UNZLIB` | `(data$)` → STRING | formato zlib |
| `BZIP2` / `UNBZIP2` | `(data$)` → STRING | bzip2 |
| `COMPRESS` / `DECOMPRESS` | `(data$, algo$)` → STRING | forma generica (algoritmo a runtime) |
| `ZIP` | `(sorgenti$, dest$)` → BIT | crea archivio (sorgenti separate da `;`) |
| `UNZIP` | `(archivio$, cartella$)` → BIT | estrae archivio |

ZIP   "/tmp/a.txt" TO "/tmp/a.zip"
UNZIP "/tmp/a.zip" TO "/tmp/estratto"


## 2. Round-trip per algoritmo

IMPORT COMPRESSION AS C
SUB Main()
    s$ = "The quick brown fox jumps over the lazy dog."
    PRINT C.UNGZIP(C.GZIP(s$))
    PRINT C.UNDEFLATE(C.DEFLATE(s$))
    PRINT C.UNBROTLI(C.BROTLI(s$))
    PRINT C.UNZLIB(C.ZLIB(s$))
    PRINT C.UNBZIP2(C.BZIP2(s$))
END SUB


PRINT C.ALGORITHMS()   ' GZIP,DEFLATE,BROTLI,ZLIB,BZIP2


## 3. Forma generica COMPRESS / DECOMPRESS

z$ = C.COMPRESS(s$, "GZIP")
PRINT C.DECOMPRESS(z$, "GZIP")


## 4. Archivi ZIP

IMPORT IO
IMPORT COMPRESSION AS C
SUB Main()
    DIM h AS LONG
    h = IO.FCREATE("/tmp/a.txt")
    IO.FWRITE(h, "contenuto" + CHR(10))
    IO.FCLOSE(h)

    ZIP   "/tmp/a.txt" TO "/tmp/a.zip"
    UNZIP "/tmp/a.zip" TO "/tmp/out"

    DIM lines = IO.READLINES("/tmp/out/a.txt")
    PRINT lines(0)   ' contenuto
END SUB


## 5. Note di parità

## 6. MiniTest e sample

<!-- END SURFACE -->
