' FILE: Clipper/ClipperEvalLocals.bas
' =================================================================================================
'   DEMO — EVAL sulle LOCALI del chiamante (S_eval, semantica vincolante 2026-09-09).
'   Supportato su PCodeVM, VM/C, IL (Gap B: snapshot CLR) e C99/C99Native (frame
'   S_eval per sito macro). Lookup locali-prima, write-back in place, shadowing.
'   Nota storica: su IL/C99 era un gap strutturale (locali CLR / variabili C)
'   chiuso il 2026-09-09 (vedi Todo parità EVAL).
'   Per il contenimento puro (senza locali) vedi ClipperEvalScope.bas.
' =================================================================================================
GLOBAL g AS INTEGER

SUB Helper(p AS INTEGER)
    DIM r AS INTEGER
    r = EVAL("p * 10")
    IF r = 50 THEN PRINT "locals-param-ok" ELSE PRINT "locals-param-FAIL"
END SUB

SUB Main()
    g = 100
    DIM x AS INTEGER
    x = 10
    DIM ok AS INTEGER
    ok = 0
    DIM dummy AS INTEGER
    DIM t AS INTEGER

    ' 1. lettura di una locale
    t = EVAL("x + 5")
    IF t = 15 THEN ok = ok + 1

    ' 2. scrittura di una locale (write-back)
    dummy = EVAL("x = 77")
    IF x = 77 THEN ok = ok + 1

    ' 3. combinazione locale + globale
    t = EVAL("x + g")
    IF t = 177 THEN ok = ok + 1

    ' 4. lettura di un parametro
    Helper(5)

    PRINT "locals-cases-ok:"; ok
    IF ok = 3 THEN PRINT "locals-ALL-OK" ELSE PRINT "locals-FAIL"
END SUB
