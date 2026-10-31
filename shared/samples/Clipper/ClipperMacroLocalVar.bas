' FILE: Clipper/ClipperMacroLocalVar.bas
' =================================================================================================
'   CANARINO — EVAL/& su una variabile LOCALE del SUB corrente (scope rispettato).
'   Verifica che la macro/EVAL veda le LOCALI della procedura che la invoca (oltre alle globali).
'   Backend: PCodeVM, IL, VM/C (frame S_eval) — tutti 7/7 dal 2026-09-09.
' =================================================================================================
GLOBAL g AS INTEGER

SUB Main()
    g = 100
    DIM localInt AS INTEGER
    localInt = 10

    ' 1. EVAL legge una LOCALE
    DIM a AS INTEGER
    a = EVAL("localInt + 5")
    PRINT "local-eval-read:"; a
    IF a = 15 THEN PRINT "local-eval-read-ok" ELSE PRINT "local-eval-read-FAIL"

    ' 2. & legge una LOCALE
    DIM b AS INTEGER
    b = &("localInt * 2")
    PRINT "local-macro-read:"; b
    IF b = 20 THEN PRINT "local-macro-read-ok" ELSE PRINT "local-macro-read-FAIL"

    ' 3. EVAL legge una GLOBALE (controllo: deve funzionare ovunque)
    DIM c AS INTEGER
    c = EVAL("g + 1")
    PRINT "local-eval-global:"; c
    IF c = 101 THEN PRINT "local-eval-global-ok" ELSE PRINT "local-eval-global-FAIL"

    ' 4. EVAL combina LOCALE + GLOBALE
    DIM d AS INTEGER
    d = EVAL("localInt + g")
    PRINT "local-eval-both:"; d
    IF d = 110 THEN PRINT "local-eval-both-ok" ELSE PRINT "local-eval-both-FAIL"

    ' 5. EVAL scrive una LOCALE (assegnazione; EVAL come statement richiede un LHS)
    DIM e AS INTEGER
    DIM dummy AS INTEGER
    dummy = EVAL("localInt = 77")
    PRINT "local-eval-write:"; localInt
    IF localInt = 77 THEN PRINT "local-eval-write-ok" ELSE PRINT "local-eval-write-FAIL"

    ' 6. LET in EVAL crea in S_eval (shadow): la locale esterna resta intatta
    ' (confronto contro il valore corrente, non una costante: su backend senza cattura
    '  locali il caso 5 fallisce e localInt vale ancora 10 — lo shadow si verifica comunque)
    DIM beforeShadow AS INTEGER
    beforeShadow = localInt
    dummy = EVAL("LET localInt = 999")
    PRINT "local-eval-let-shadow:"; localInt
    IF localInt = beforeShadow THEN PRINT "local-eval-let-shadow-ok" ELSE PRINT "local-eval-let-shadow-FAIL"

    ' 7. implicita-su-ignota in EVAL non leak (crea in S_eval, muore lì)
    dummy = EVAL("evalOnlyVar = 111")
    DIM evalOnlyCheck AS INTEGER
    evalOnlyCheck = EVAL("evalOnlyVar")
    PRINT "local-eval-noleak:"; evalOnlyCheck
    IF evalOnlyCheck = 111 THEN PRINT "local-eval-noleak-FAIL" ELSE PRINT "local-eval-noleak-ok"
END SUB
