' FILE: compression/CompressionBasics.bas
' ============================================================================================================
' COMPRESSION — compressione stringhe con TUTTI gli algoritmi (GZIP, DEFLATE, BROTLI, ZLIB, BZIP2)
' ============================================================================================================
IMPORT COMPRESSION WITH ALIAS

SUB Main()
    PRINT "=== Compressione Stringhe ==="
    DIM testo$ = "Hello GMBasic Compression! Questo e un testo ripetuto per rendere la compressione efficace." + " Seconda parte del testo di prova per il roundtrip."
    PRINT "Originale len: "; LEN(testo$)
    PRINT

    ' 1. GZIP
    DIM gz$ = COMPRESSION.GZIP(testo$)
    PRINT "GZIP: "; LEN(gz$); " chars (base64)"
    PRINT "  roundtrip: "; COMPRESSION.UNGZIP(gz$) = testo$
    PRINT

    ' 2. DEFLATE
    DIM df$ = COMPRESSION.DEFLATE(testo$)
    PRINT "DEFLATE: "; LEN(df$); " chars"
    PRINT "  roundtrip: "; COMPRESSION.UNDEFLATE(df$) = testo$
    PRINT

    ' 3. BROTLI
    DIM br$ = COMPRESSION.BROTLI(testo$)
    PRINT "BROTLI: "; LEN(br$); " chars"
    PRINT "  roundtrip: "; COMPRESSION.UNBROTLI(br$) = testo$
    PRINT

    ' 4. ZLIB
    DIM zl$ = COMPRESSION.ZLIB(testo$)
    PRINT "ZLIB: "; LEN(zl$); " chars"
    PRINT "  roundtrip: "; COMPRESSION.UNZLIB(zl$) = testo$
    PRINT

    ' 5. BZIP2
    DIM bz$ = COMPRESSION.BZIP2(testo$)
    PRINT "BZIP2: "; LEN(bz$); " chars"
    PRINT "  roundtrip: "; COMPRESSION.UNBZIP2(bz$) = testo$
    PRINT

    ' 6. Generico COMPRESS / DECOMPRESS con algoritmo
    DIM c1$ = COMPRESSION.COMPRESS(testo$, "GZIP")
    PRINT "COMPRESS(.., 'GZIP') roundtrip: "; COMPRESSION.DECOMPRESS(c1$, "GZIP") = testo$
    PRINT

    ' 7. Elenco algoritmi
    PRINT "Algoritmi supportati: "; COMPRESSION.ALGORITHMS()
    PRINT
    PRINT "Test completato."
END SUB
