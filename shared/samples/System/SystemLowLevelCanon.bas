' FILE: System/SystemLowLevelCanon.bas
' =================================================================================================
'       TEST: SYSTEM LOW-LEVEL — CANONE DI PARITA 4 BACKEND (2026-09-30)
' =================================================================================================
' Esercita in UN solo programma completo tutti i comandi low-level del canone:
'   SYSTEM.TI() / SYSTEM.STRTI() / SYSTEM.VARPTR() / SYSTEM.STRPTR()
' piu la composizione booleana canonica (AND/OR/XOR su confronti = -1/0).
' Stampa SOLO proprieta deterministiche (-1/0/8): l'output e byte-identico su
' IL, PCodeVM, VM/C, C99Native (verificato con tools/parity4).
' Niente valori assoluti (indirizzi, millisecondi, ora) nell'output confrontato.
' =================================================================================================

IMPORT SYSTEM
IMPORT CONVERSIONS
IMPORT CONCURRENCY

SUB Main()
    PRINT "--- 1. Token di identita (mai 0, stabili) ---"
    DIM i AS INTEGER = 123
    DIM j AS INTEGER = 456
    DIM s AS STRING = "Hello"
    PRINT "vp-nonzero ="; (SYSTEM.VARPTR(i) <> 0)
    PRINT "sp-nonzero ="; (SYSTEM.STRPTR(s) <> 0)
    PRINT "vp-stable ="; (SYSTEM.VARPTR(i) = SYSTEM.VARPTR(i))
    PRINT "sp-stable ="; (SYSTEM.STRPTR(s) = SYSTEM.STRPTR(s))
    PRINT "vp-distinct ="; (SYSTEM.VARPTR(i) <> SYSTEM.VARPTR(j))
    PRINT

    PRINT "--- 2. Timer in millisecondi (solo delta portabili) ---"
    DIM t1 = SYSTEM.TI()
    SLEEP 100
    DIM t2 = SYSTEM.TI()
    PRINT "ti-mono ="; (t2 >= t1)
    PRINT "ti-ms-class ="; ((t2 - t1) >= 50 AND (t2 - t1) < 3600000)
    PRINT

    PRINT "--- 3. Ora di sistema HH:MM:SS ---"
    PRINT "strti-len ="; LEN(SYSTEM.STRTI())
    PRINT "strti-sep ="; ((CONVERSIONS.MID(SYSTEM.STRTI(), 3, 1) = ":") AND (CONVERSIONS.MID(SYSTEM.STRTI(), 6, 1) = ":"))
    PRINT

    PRINT "--- 4. Booleani canonici (-1/0) ---"
    PRINT "and-tt ="; ((2 <> 0) AND (3 <> 0))
    PRINT "and-ft ="; ((2 = 0) AND (3 <> 0))
    PRINT "or-ft ="; ((2 = 0) OR (3 <> 0))
    PRINT "xor-tf ="; ((2 <> 0) XOR (3 = 0))
    PRINT "and-val ="; ((2 <> 0) AND 5)
    PRINT "str-and ="; (("a" = "a") AND (1 <> 0))
    PRINT

    PRINT "Canon-OK."
END SUB
