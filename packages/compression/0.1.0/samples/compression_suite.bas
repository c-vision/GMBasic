' compression_suite.bas — pacchetto `compression` (FFI su libgmbcompression).
'
' Esempio completo del dominio: usa TUTTE le 19 funzioni pubbliche del namespace
' COMPRESSION. Ogni sezione spiega input, formato e significato dell'output.
' Le funzioni di compressione restituiscono il flusso compresso in BASE64; le
' funzioni UN- riportano al testo originale.
'
' ZIP/UNZIP non sono qui: restano builtin del linguaggio (archivi), non del
' pacchetto.
'
' Esecuzione:
'   gmb pkg install --local packages/compression/0.1.0
'   gmb run shared/samples/packages/compression@0.1.0/compression_suite.bas
'
' Requisiti: pacchetto `compression` installato; zlib/zstd/lz4/xz/bzip2/brotli
' presenti sulla macchina (li linka la dylib).

REQUIRE "compression"
IMPORT COMPRESSION
IMPORT STRINGS

SUB Main()
    DIM testo AS STRING
    testo = "hello compression hello compression hello compression"

    ' ── 0. ALGORITMI DISPONIBILI ───────────────────────────────────────────
    ' Stringa con l'elenco separato da virgole: serve a scegliere un algoritmo
    ' senza cablarlo nel programma.
    PRINT "== algoritmi =="
    PRINT "supportati = "; ALGORITHMS()

    ' ── 1. API GENERICA ────────────────────────────────────────────────────
    ' COMPRESS(testo, nome) e DECOMPRESS(base64, nome) sono la porta unica:
    ' il nome è case-insensitive. Utile quando l'algoritmo è un dato.
    PRINT "== api generica =="
    DIM algo AS STRING
    algo = "GZIP"
    DIM pacchetto AS STRING
    pacchetto = COMPRESS(testo, algo)
    PRINT "algo       = "; algo
    PRINT "base64 len = "; LEN(pacchetto)
    PRINT "round-trip = "; (DECOMPRESS(pacchetto, algo) = testo)

    ' ── 2. GZIP ────────────────────────────────────────────────────────────
    ' Formato HTTP/Unix (header+trailer). È il default del dominio.
    PRINT "== gzip =="
    DIM gz AS STRING
    gz = GZIP(testo)
    PRINT "gzip len   = "; LEN(gz)
    PRINT "ungzip ok  = "; (UNGZIP(gz) = testo)

    ' ── 3. ZLIB e DEFLATE ──────────────────────────────────────────────────
    ' ZLIB = wrapper zlib (header Adler-32); DEFLATE = flusso raw senza header.
    ' DEFLATE è il più compatto ma richiede che il lettore conosca il formato.
    PRINT "== zlib / deflate =="
    DIM zl AS STRING
    DIM df AS STRING
    zl = ZLIB(testo)
    df = DEFLATE(testo)
    PRINT "zlib len   = "; LEN(zl); " ok="; (UNZLIB(zl) = testo)
    PRINT "deflate len= "; LEN(df); " ok="; (UNDEFLATE(df) = testo)

    ' ── 4. BZIP2 ───────────────────────────────────────────────────────────
    ' Compressione a blocchi: ratio migliore di gzip su testo lungo, più lenta.
    PRINT "== bzip2 =="
    DIM bz AS STRING
    bz = BZIP2(testo)
    PRINT "bzip2 len  = "; LEN(bz)
    PRINT "unbzip2 ok = "; (UNBZIP2(bz) = testo)

    ' ── 5. BROTLI ──────────────────────────────────────────────────────────
    ' Pensato per il web: ottimo ratio su HTML/testo, decodifica veloce.
    PRINT "== brotli =="
    DIM br AS STRING
    br = BROTLI(testo)
    PRINT "brotli len = "; LEN(br)
    PRINT "unbrotli ok= "; (UNBROTLI(br) = testo)

    ' ── 6. ZSTD ────────────────────────────────────────────────────────────
    ' Molto veloce e buon ratio: ideale per log e payload di rete.
    PRINT "== zstd =="
    DIM zs AS STRING
    zs = ZSTD(testo)
    PRINT "zstd len   = "; LEN(zs)
    PRINT "unzstd ok  = "; (UNZSTD(zs) = testo)

    ' ── 7. LZ4 ─────────────────────────────────────────────────────────────
    ' Il più veloce, ratio più basso: streaming e cache.
    PRINT "== lz4 =="
    DIM lz AS STRING
    lz = LZ4(testo)
    PRINT "lz4 len    = "; LEN(lz)
    PRINT "unlz4 ok   = "; (UNLZ4(lz) = testo)

    ' ── 8. XZ (LZMA) ───────────────────────────────────────────────────────
    ' Massimo ratio, compressione lenta: archivi e distribuzione.
    PRINT "== xz =="
    DIM xz AS STRING
    xz = XZ(testo)
    PRINT "xz len     = "; LEN(xz)
    PRINT "unxz ok    = "; (UNXZ(xz) = testo)

    ' ── 9. CONFRONTO ───────────────────────────────────────────────────────
    ' Tutte le coppie sono reversibili: il programma sceglie in base a
    ' velocità (LZ4/ZSTD) e dimensione (XZ/BZIP2).
    PRINT "== riepilogo =="
    PRINT "testo      = "; LEN(testo)
    PRINT "gzip/zlib/deflate/bzip2/brotli/zstd/lz4/xz = ";
    PRINT LEN(gz); "/"; LEN(zl); "/"; LEN(df); "/"; LEN(bz); "/"; LEN(br); "/"; LEN(zs); "/"; LEN(lz); "/"; LEN(xz)
END SUB
