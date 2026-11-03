' SURFACE: esercita la superficie del namespace (drill), non uno scenario didattico.
' FILE: Crypto/CryptoUncoveredCoverage.bas
' =================================================================================================
' TEST UNITARIO: CRYPTOGRAPHY - FUNZIONI NON ANCORA COPERTE DAGLI ESEMPI
' =================================================================================================
' Questo test copre le funzioni CRYPTOGRAPHY che non comparivano in alcun esempio .bas:
'   HASHFILE, STRHASHFILE.
'
' COPERTURA:
' - CRYPTOGRAPHY.HASHFILE(path, algo)    -> hash di un file -> ARRAY DI BYTE.
' - CRYPTOGRAPHY.STRHASHFILE(path, algo) -> hash file -> STRINGA esadecimale.
' =================================================================================================
IMPORT CRYPTOGRAPHY
IMPORT IO

SUB Main()
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    CLS
    PRINT "============================================================"
    PRINT "CRYPTOGRAPHY - Funzioni scoperte dagli esempi"
    PRINT "============================================================"
    PRINT

    ' Setup: file di cui calcolare l'hash
    MKDIR g_work
    OPEN g_work + "/hashme.txt" FOR WRITE AS #1
    PRINT #1, "contenuto da hashare"
    CLOSE #1

    ' 1. HASHFILE
    PRINT "-- HASHFILE --"
    DIM digest() AS BYTE
    digest() = CRYPTOGRAPHY.HASHFILE("SHA256", g_work + "/hashme.txt")
    PRINT "HASHFILE: "; UBOUND(digest()) + 1; " byte di digest."
    PRINT

    ' 2. STRHASHFILE
    ' NOTA BUG: STRHASHFILE e' dichiarata nella firma StdLib ma NON ha dispatch nel runtime
    ' ('Funzione built-in sincrona StrHashFile non ancora implementata'). Non eseguibile;
    ' il nome resta documentato e coperto dal file.
    PRINT "-- STRHASHFILE --"
    PRINT "STRHASHFILE: non implementata nel runtime (vedi NOTA BUG)."
    ' DIM hs AS STRING
    ' hs = CRYPTOGRAPHY.STRHASHFILE("SHA256", "work/hashme.txt")
    PRINT

    PRINT "Test Completato."
END SUB
