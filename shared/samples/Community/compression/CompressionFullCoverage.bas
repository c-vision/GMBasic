' FILE: compression/CompressionFullCoverage.bas
' Copertura completa delle funzioni compression non ancora coperte.
IMPORT COMPRESSION

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    CLS
    PRINT "============================================================"
    PRINT "Compression - Copertura Completa"
    PRINT "============================================================"
    PRINT

    DIM src(1) AS STRING
    src(0) = "sequential.txt"
    DIM ok = COMPRESSION.ZIP(src, g_work + "/test.zip")
    PRINT "ZIP ok: "; ok
    DIM ok2 = COMPRESSION.UNZIP(g_work + "/test.zip", g_work + "/zipout")
    PRINT "UNZIP ok: "; ok2
    PRINT

    PRINT "Test Completato."
END SUB
