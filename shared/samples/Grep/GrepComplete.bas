' FILE: Grep/GrepComplete.bas
' GREP — uso COMPLETO e complesso delle 3 funzioni (array MATCHES/FILES + COUNT):
' conteggio, iterazione degli array, pattern, verifica di coerenza.
IMPORT GREP

SUB Main()
    DIM dir AS STRING
    dir = PROJECTFOLDER()
    PRINT "================================================"
    PRINT "GREP - uso completo su "; dir
    PRINT "================================================"

    DIM pat AS STRING
    pat = "GREP"

    ' 1) COUNT: numero di hit
    PRINT "1) COUNT(GREP) = "; GREP.COUNT(pat, dir)

    ' 2) MATCHES: ARRAY di "file:riga:col:testo" + iterazione
    DIM m() AS STRING
    m = GREP.MATCHES(pat, dir)
    PRINT "2) MATCHES -> "; UBOUND(m) + 1; " hit:"
    FOR i = 0 TO UBOUND(m)
        PRINT "   "; m[i]
    NEXT

    ' 3) FILES: ARRAY di file distinti + iterazione
    DIM f() AS STRING
    f = GREP.FILES("IMPORT GREP", dir)
    PRINT "3) FILES(IMPORT GREP) -> "; UBOUND(f) + 1; " file:"
    FOR i = 0 TO UBOUND(f)
        PRINT "   "; f[i]
    NEXT

    ' 4) coerenza: COUNT == numero di elementi di MATCHES
    PRINT "4) Coerenza COUNT==MATCHES: "; GREP.COUNT(pat, dir) = UBOUND(m) + 1

    ' 5) pattern assente -> 0 hit, array vuoto (costruito a runtime per NON auto-matchare il sorgente)
    DIM pat2 AS STRING
    pat2 = "zzz" + "_inesistente_zzz"
    DIM e() AS STRING
    e = GREP.FILES(pat2, dir)
    PRINT "5) Pattern assente -> COUNT="; GREP.COUNT(pat2, dir); " FILES="; UBOUND(e) + 1
    PRINT "================================================"
    PRINT "GREP completo OK"
END SUB
