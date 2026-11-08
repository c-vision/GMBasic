' FILE: compression/CompressionAlgorithms.bas
' COMPRESSION — algoritmi aggiunti (2026-10-10): ZSTD/UNZSTD, LZ4/UNLZ4, XZ/UNXZ.
' Round-trip di ciascun algoritmo + verifica via COMPRESS/DECOMPRESS per nome.
REQUIRE "compression"
IMPORT COMPRESSION AS C

SUB Main()
    DIM testo AS STRING
    testo = "The quick brown fox jumps over the lazy dog. Compression algorithms test. The quick brown fox jumps over the lazy dog. Compression algorithms test."

    PRINT "Testo originale: "; LEN(testo); " byte"
    PRINT "================================================"

    DIM z AS STRING
    z = C.ZSTD(testo)
    PRINT "ZSTD  compresso: "; LEN(z); " byte  round-trip: "; C.UNZSTD(z) = testo
    PRINT "LZ4   compresso: "; LEN(C.LZ4(testo)); " byte  round-trip: "; C.UNLZ4(C.LZ4(testo)) = testo
    PRINT "XZ    compresso: "; LEN(C.XZ(testo)); " byte  round-trip: "; C.UNXZ(C.XZ(testo)) = testo

    PRINT "================================================"
    PRINT "Via COMPRESS/DECOMPRESS per nome:"
    PRINT "  zstd round-trip: "; C.DECOMPRESS(C.COMPRESS(testo, "zstd"), "zstd") = testo
    PRINT "  lz4  round-trip: "; C.DECOMPRESS(C.COMPRESS(testo, "lz4"), "lz4") = testo
    PRINT "  xz   round-trip: "; C.DECOMPRESS(C.COMPRESS(testo, "xz"), "xz") = testo
    PRINT
    PRINT "Algoritmi disponibili: "; C.ALGORITHMS()
END SUB
