' FILE: Clipper/ClipperEvalScope.bas
' =================================================================================================
'   DEMO — S_eval: lo scope transiente di EVAL (semantica vincolante 2026-09-09).
'   I casi 1,2,3,5 valgono su TUTTI i backend (IL, PCodeVM, VM/C, C99): nessuna lettura di
'   locali del chiamante, solo contenimento/shadowing/write-back su globali/valori.
'   Il caso 4 (EVAL annidato) è INFORMATIVO: 43 su PCodeVM/VM-C/C99, 1 su IL per il bug
'   di piattaforma GPM-worker-scope (vedi Bugs/(open) - IL-GPM-WorkerScope-Loss-Globali-Macro).
'   Per le locali del chiamante vedi ClipperEvalLocals.bas (solo PCodeVM + VM/C).
' =================================================================================================
GLOBAL g AS INTEGER

SUB Main()
    g = 10
    DIM outer AS INTEGER
    outer = 5
    DIM dummy AS INTEGER
    DIM ok AS INTEGER
    ok = 0

    ' 1. LET in EVAL crea in S_eval (shadow): l'esterna resta intatta
    dummy = EVAL("LET outer = 999")
    IF outer = 5 THEN ok = ok + 1

    ' 2. implicita-su-ignota in EVAL non leak (muore con lo scope)
    dummy = EVAL("ghostVar = 123")
    DIM probe AS INTEGER
    probe = EVAL("ghostVar")
    IF probe <> 123 THEN ok = ok + 1

    ' 3. scrittura su globale preesistente persiste (write-back)
    dummy = EVAL("g = 42")
    IF g = 42 THEN ok = ok + 1

    ' 4. EVAL annidato (catena di scope, sul globale) — INFORMATIVO (vedi header)
    DIM q AS STRING
    q = CHR(34)
    DIM n AS INTEGER
    n = EVAL("EVAL(" + q + "g" + q + ") + 1")
    PRINT "scope-nested:"; n

    ' 5. valore di ritorno dell'espressione
    DIM v AS INTEGER
    v = EVAL("6 * 7")
    IF v = 42 THEN ok = ok + 1

    PRINT "scope-cases-ok:"; ok
    IF ok = 4 THEN PRINT "scope-ALL-OK" ELSE PRINT "scope-FAIL"
END SUB
