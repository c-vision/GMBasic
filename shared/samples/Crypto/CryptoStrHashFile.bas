' FILE: Crypto/CryptoStrHashFile.bas
' =================================================================================================
' CRYPTOGRAPHY.STRHASHFILE(path$, algo$) — hash di file come stringa esadecimale
' NB (2026-11-13): CRYPTOGRAPHY è un PACCHETTO FFI — serve `REQUIRE "crypto"` prima di IMPORT
' (senza, il namespace non è risolto e ogni simbolo risulta "non definito").
' =================================================================================================
REQUIRE "crypto"
IMPORT CRYPTOGRAPHY

SUB Main()
    ' CLS/TUI non necessari: il sample deve girare anche headless.
    PRINT "=== CRYPTOGRAPHY.STRHASHFILE ==="
    IF NOT IO.DIREXISTS("work") THEN MKDIR "work"
    DIM testFile$ = "work/hashme.txt"
    DIM path$ = testFile$

    ' Crea file di test se non esiste
    IF NOT FILEEXISTS(testFile$) THEN
        OPEN testFile$ FOR OUTPUT AS #1
        WRITE #1, "contenuto di test"
        CLOSE #1
    END IF

    DIM hash$ = CRYPTOGRAPHY.STRHASHFILE(path$, "SHA256")
    PRINT "SHA256 hash: "; hash$

    DIM hashMd5$ = CRYPTOGRAPHY.STRHASHFILE(path$, "MD5")
    PRINT "MD5 hash   : "; hashMd5$

    PRINT "=== OK ==="
END SUB
