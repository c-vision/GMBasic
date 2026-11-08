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














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### COMPRESSION-Guida-Utente

*Fonte: `COMPRESSION-Guida-Utente`*


## 1. Installazione e import

gmb pkg install --local packages/compression/0.1.0    # sviluppo
gmb pkg install compression                            # dal registry


REQUIRE "compression"      ' porta export + libreria del pacchetto
IMPORT COMPRESSION         ' funzioni stream/algoritmi
IMPORT ARCHIVE             ' solo se servono ZIP/UNZIP in forma funzione


## 2. Funzioni di compressione (namespace `COMPRESSION`, pacchetto)

### Generiche
| funzione | parametri | ritorno |
|---|---|---|
| `COMPRESSION.COMPRESS(data$, algorithm$)` | `data$` testo, `algorithm$` nome algoritmo | STRING (Base64) |
| `COMPRESSION.DECOMPRESS(data$, algorithm$)` | `data$` Base64, `algorithm$` nome | STRING (testo) |
| `COMPRESSION.ALGORITHMS()` | — | STRING `GZIP,DEFLATE,BROTLI,ZLIB,BZIP2,ZSTD,LZ4,XZ` |

### Coppie dedicate (comprimi/decomprimi)
| comprimi | decomprimi | libreria |
|---|---|---|
| `COMPRESSION.GZIP(data$)` | `COMPRESSION.UNGZIP(data$)` | zlib |
| `COMPRESSION.ZLIB(data$)` | `COMPRESSION.UNZLIB(data$)` | zlib |
| `COMPRESSION.DEFLATE(data$)` | `COMPRESSION.UNDEFLATE(data$)` | zlib |
| `COMPRESSION.BZIP2(data$)` | `COMPRESSION.UNBZIP2(data$)` | libbz2 |
| `COMPRESSION.BROTLI(data$)` | `COMPRESSION.UNBROTLI(data$)` | brotli |
| `COMPRESSION.ZSTD(data$)` | `COMPRESSION.UNZSTD(data$)` | libzstd |
| `COMPRESSION.LZ4(data$)` | `COMPRESSION.UNLZ4(data$)` | liblz4 |
| `COMPRESSION.XZ(data$)` | `COMPRESSION.UNXZ(data$)` | liblzma |

### Parametri (dettaglio)

## 3. Archivi ZIP/UNZIP (namespace `ARCHIVE`, builtin)
| forma | sintassi | effetto |
|---|---|---|
| comando | `ZIP sources TO dest` | crea archivio ZIP (STORE) |
| comando | `UNZIP archive TO dir` | estrae l'archivio |
| funzione | `ARCHIVE.ZIP(sources$, dest$)` | crea → BIT (-1 vero / 0 errore) |
| funzione | `ARCHIVE.UNZIP(archive$, dir$)` | estrae → BIT |

## 4. Esempio

REQUIRE "compression"
IMPORT COMPRESSION
IMPORT ARCHIVE

SUB Main()
    DIM s AS STRING
    s = "The quick brown fox..."

    ' generico
    DIM z AS STRING
    z = COMPRESS(s, "zstd")
    PRINT "round-trip: "; DECOMPRESS(z, "zstd") = s

    ' dedicato
    PRINT "zstd: "; UNZSTD(ZSTD(s)) = s
    PRINT "lz4:  "; UNLZ4(LZ4(s)) = s


### Polimorfismo / console-aware

## 5. Note implementative

## 6. Riferimenti

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




<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **ARCHIVE.UNZIP(x1, x2)** — `ARCHIVE.UNZIP(tmpZip, staging)` (shared/samples/PackageRegistry/PackageServer.bas:1789)
- **ARCHIVE.ZIP(x1, x2)** — `ok = ARCHIVE.ZIP(srcs, tmp)` (shared/samples/PackageRegistry/PackageServer.bas:487)
- **BITS.BITCLEAR(x1, x2)** — `RETURN BITS.BITCLEAR(mask, bit)` (shared/samples/Bits/PermessiUtente.bas:20)
- **BITS.BITCOUNT(x1)** — `PRINT "ospite:    "; ospite; "    permessi attivi: "; BITS.BITCOUNT(ospite)` (shared/samples/Bits/PermessiUtente.bas:60)
- **BITS.BITDEINTERLEAVE(x1, x2)** — `IF BITS.BITDEINTERLEAVE(indice, 0) <> x THEN errori = errori + 1` (shared/samples/Bits/ProtocolloPacchetti.bas:85)
- **BITS.BITFIELD(x1, x2, x3)** — `RETURN BITS.BITFIELD(header, da, a)` (shared/samples/Bits/ProtocolloPacchetti.bas:23)
- **BITS.BITGET(x1, x2)** — `PRINT "admin può cancellare (BITGET): "; BITS.BITGET(admin, 3)` (shared/samples/Bits/PermessiUtente.bas:103)
- **BITS.BITINTERLEAVE(x1, x2)** — `indice = BITS.BITINTERLEAVE(x, y)` (shared/samples/Bits/ProtocolloPacchetti.bas:84)
- **BITS.BITREVERSE(x1, x2)** — `cifrato = BITS.BITROTATEL(BITS.BITREVERSE(chiaro, 8), 3, 8)` (shared/samples/Bits/ProtocolloPacchetti.bas:100)
- **BITS.BITROTATEL(x1, x2, x3)** — `cifrato = BITS.BITROTATEL(BITS.BITREVERSE(chiaro, 8), 3, 8)` (shared/samples/Bits/ProtocolloPacchetti.bas:100)
- **BITS.BITROTATER(x1, x2, x3)** — `decifrato = BITS.BITREVERSE(BITS.BITROTATER(cifrato, 3, 8), 8)` (shared/samples/Bits/ProtocolloPacchetti.bas:102)
- **BITS.BITSCANFWD(x1)** — `PRINT "bit più basso di 40: "; BITS.BITSCANFWD(40); _` (shared/samples/Bits/ProtocolloPacchetti.bas:118)
- **BITS.BITSCANREV(x1)** — `", più alto: "; BITS.BITSCANREV(40)` (shared/samples/Bits/ProtocolloPacchetti.bas:119)
- **BITS.BITSET(x1, x2)** — `RETURN BITS.BITSET(mask, bit)` (shared/samples/Bits/PermessiUtente.bas:16)
- **BITS.BITTEST(x1, x2)** — `RETURN BITS.BITTEST(mask, bit)` (shared/samples/Bits/PermessiUtente.bas:24)
- **BITS.BITTOGGLE(x1, x2)** — `impiegato = BITS.BITTOGGLE(impiegato, 3)` (shared/samples/Bits/PermessiUtente.bas:95)
- **BITS.BYTESWAP(x1)** — `PRINT "byteswap due volte è l'identità: "; (BITS.BYTESWAP(BITS.BYTESWAP(12345)) = 12345)` (shared/samples/Bits/ProtocolloPacchetti.bas:105)
- **BITS.FROMGRAY(x1)** — `IF BITS.FROMGRAY(BITS.GRAYCODE(i)) <> i THEN violazioni = violazioni + 1` (shared/samples/Bits/ProtocolloPacchetti.bas:67)
- **BITS.GRAYCODE(x1)** — `IF BITS.FROMGRAY(BITS.GRAYCODE(i)) <> i THEN violazioni = violazioni + 1` (shared/samples/Bits/ProtocolloPacchetti.bas:67)
- **BITS.HAMMINGDIST(x1, x2)** — `PRINT "distanza di Hamming fra i due ruoli: "; BITS.HAMMINGDIST(admin, ospite)` (shared/samples/Bits/PermessiUtente.bas:113)
- **BITS.ISPOW2(x1)** — `PRINT "buffer: "; capacita; " byte (potenza di 2: "; BITS.ISPOW2(capacita); ")"` (shared/samples/Bits/ProtocolloPacchetti.bas:116)
- **BITS.NEXTPOW2(x1)** — `capacita = BITS.NEXTPOW2(bisogno)` (shared/samples/Bits/ProtocolloPacchetti.bas:115)
- **BITS.PARITY(x1)** — `parita = parita + BITS.PARITY(pacchetti(i))` (shared/samples/Bits/ProtocolloPacchetti.bas:53)
<!-- END DOMAIN TABLE -->
