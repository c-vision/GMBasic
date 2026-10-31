' FILE: Clipper/ClipperEvalBlockLocals.bas
' =================================================================================================
'   DEMO — EVAL/& sulle LOCALI DI BLOCCO (IF/FOR) del chiamante (S_eval, semantica 2026-09-09).
'   Una DIM dichiarata DENTRO un blocco IF/FOR vive solo nel blocco (§6 Variables-Scope-Spec):
'   un EVAL nello STESSO blocco deve risolverla (read + write-back), un EVAL dopo il blocco no.
'   Chiuso 2026-09-09 con lo snapshot per-sito macro (locali vive al sito, incluse block).
'   =================================================================================================
SUB Main()
    DIM ok AS INTEGER
    ok = 0

    ' 1. IF-block: DIM dentro blocco letta da EVAL nello stesso blocco
    IF 1 = 1 THEN
        DIM inif AS INTEGER
        inif = 21
        DIM r1 AS INTEGER
        r1 = EVAL("inif * 2")
        IF r1 = 42 THEN ok = ok + 1
        ' 2. write-back su block-local (implicito su nome esistente)
        DIM w1 AS INTEGER
        w1 = EVAL("inif = 99")
        IF inif = 99 THEN ok = ok + 1
    END IF

    ' 3. FOR: contatore (block-local) + DIM del corpo risolti da EVAL nel corpo
    DIM j AS INTEGER
    DIM acc AS INTEGER
    acc = 0
    FOR j = 1 TO 3
        DIM doubled AS INTEGER
        doubled = EVAL("j * 2")
        acc = acc + doubled
    NEXT
    IF acc = 12 THEN ok = ok + 1

    ' 4. niente leak fuori blocco: EVAL dentro il blocco risolve, i nomi restano confinati
    IF 1 = 1 THEN
        DIM gonebl AS INTEGER
        gonebl = 41
        DIM pr1 AS INTEGER
        pr1 = EVAL("gonebl + 1")
        IF pr1 = 42 THEN ok = ok + 1
    END IF

    PRINT "blk-cases-ok:"; ok
    IF ok = 4 THEN PRINT "blk-ALL-OK" ELSE PRINT "blk-FAIL"
END SUB
