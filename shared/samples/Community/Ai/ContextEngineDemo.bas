' Sample AI CONTEXT — Context Engine (P2). Dimostra budget, view, letture.
IMPORT AI

SUB Main()
    PRINT "=== AI CONTEXT Engine demo ==="
    PRINT ""

    ' 1. SET BUDGET — imposta il budget contesto per un agente
    AI CONTEXT SET BUDGET "worker1", 16000
    PRINT "1. Budget impostato: " + STR$(AI.CONTEXTBUDGET("worker1"))

    ' SELECTOR: quali sezioni dello STATE entrano nella vista (v1: accettato e conservato;
    ' l'applicazione effettiva è tracciata nel todo AI-Runtime, asse B/D4). Il comando è
    ' esercitato qui perché faccia parte della superficie coperta dal sample.
    AI CONTEXT SET SELECTOR "worker1", "task,deps,freshness"
    AI CONTEXT SET POLICY "worker1", "compact"
    PRINT "1b. Selector/policy impostati"

    ' 2. VIEW — imposta la vista curata
    AI CONTEXT VIEW "worker1", "plan: solve problem, notes: use greedy, solution: main.c"
    DIM view$ = AI.CONTEXTVIEW$("worker1")
    PRINT "2. View curata: " + LEFT$(view$, 60) + "..."

    ' 3. CONTEXTUSED — caratteri usati dalla view
    PRINT "3. Caratteri usati: " + STR$(AI.CONTEXTUSED("worker1"))

    ' 4. OVERFLOW — caratteri oltre il budget (0 se view < budget)
    PRINT "4. Overflow: " + STR$(AI.CONTEXTOVERFLOW("worker1"))

    ' 5. VIEWDEBUG — debug view (v1 uguale a VIEW)
    DIM dbg$ = AI.CONTEXTVIEWDEBUG$("worker1")
    PRINT "5. View debug ok"

    ' 6. Budget di default (agente non impostato)
    PRINT "6. Budget default: " + STR$(AI.CONTEXTBUDGET("nobody"))

    PRINT ""
    PRINT "=== Fine demo ==="
END SUB