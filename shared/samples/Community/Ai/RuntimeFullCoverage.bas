' RUNTIME-FULL-COVERAGE — sample COMPLETO del filone AI-Runtime (P1 OBSERVE + P2 CONTEXT +
' P3/P4 WORKER): usa OGNI comando e funzione implementata in questa sessione, con output
' deterministico su tutte e 5 le gambe (IL, PCodeVM, VM/C, C99Native).
'
' Copre:
'   • AI CONTEXT SET BUDGET / SET POLICY / SET SELECTOR / VIEW  +  AI.CONTEXTUSED/BUDGET/OVERFLOW/
'     VIEW$ (vista curata normalizzata secondo budget+policy)
'   • AI WORKER NEW (workspace + budget del run) / RUN / "<TASK>" (invocazione pulita) /
'     LOOP [, rounds] [REQUIRE "…"] (loop isolato multi-round, budget CUMULATIVO, auditor
'     indipendente, best-so-far)
'   • AI STATE SAVE / AI.STATELOAD (memoria esterna del run)  +  AI.WORKERROUNDS/RESCUED/SPEND
'   • AI OBSERVE FILE / LOG / SCREEN  +  AI.OBSERVATIONCOUNT / LATEST$ / HASH$ / LIST$ / FILE$ /
'     OBSERVATION$ (registry dei ContextObject)
'
' Esiti attesi senza provider LLM: le invocazioni producono l'errore ESPLICITO e il loop lo
' registra (state/last_error); con l'audit `REQUIRE` che accetta quel testo il run risulta
' VERIFICATO — è il comportamento deterministico in CI, mai un risultato finto.
' Marker finale: RUNTIME-FULL-COVERAGE-OK
IMPORT AI

SUB Main()
    DIM fixture AS STRING
    fixture = "../../../samples/Fixtures/ai/fattura_it.txt"

    ' ══ 1. CONTEXT ENGINE: budget, policy, selector, vista curata ═══════════════════
    PRINT "=== 1. CONTEXT ENGINE ==="
    AI CONTEXT SET BUDGET "worker1", 16000
    PRINT "1.1 budget= " + STR$(AI.CONTEXTBUDGET("worker1"))
    AI CONTEXT SET POLICY "worker1", "compact"
    AI CONTEXT SET SELECTOR "worker1", "task,deps,freshness"
    AI CONTEXT VIEW "worker1", "plan,notes,solution,task1"
    PRINT "1.2 view= " + AI.CONTEXTVIEW$("worker1")
    PRINT "1.3 used= " + STR$(AI.CONTEXTUSED("worker1"))
    PRINT "1.4 overflow= " + STR$(AI.CONTEXTOVERFLOW("worker1"))

    ' policy truncate: la vista è normalizzata SUBITO (taglio alla lunghezza del budget)
    AI CONTEXT SET BUDGET "worker2", 4
    AI CONTEXT SET POLICY "worker2", "truncate"
    AI CONTEXT VIEW "worker2", "1234567890"
    PRINT "1.5 truncate= " + AI.CONTEXTVIEW$("worker2")

    ' ══ 2. WORKER: workspace, stato, invocazione pulita ═════════════════════════════
    PRINT "=== 2. WORKER ==="
    AI WORKER NEW "solver", "{""workspace"":""work/rfc_solver"",""budget"":4000}"
    AI STATE SAVE "solver", "notes", "greedy algorithm: scambio locale ottimo"
    AI STATE SAVE "solver", "plan", "1) leggere  2) risolvere  3) verificare"
    PRINT "2.1 plan= " + AI.STATELOAD("solver", "plan")
    AI WORKER RUN "solver"
    PRINT "2.2 run= ok"

    AI WORKER "IMPLEMENT"
    DIM v AS STRING
    v = AI.CONTEXTVIEW$("worker:solver")
    IF INSTR(v, "TASK: IMPLEMENT") > 0 THEN PRINT "2.3 vista:task" ELSE PRINT "2.3 vista:senza-task"
    IF INSTR(v, "greedy algorithm") > 0 THEN PRINT "2.4 vista:memoria-run" ELSE PRINT "2.4 vista:senza-memoria"
    IF INSTR(v, "user:") = 0 THEN PRINT "2.5 isolamento: ok" ELSE PRINT "2.5 isolamento: no"
    IF INSTR(AI.STATELOAD("solver", "last_error"), "provider") > 0 THEN PRINT "2.6 provider: errore esplicito" ELSE PRINT "2.6 provider: attivo"

    ' ══ 3. LOOP VERIFICATO: auditor indipendente + budget cumulativo del run ════════
    PRINT "=== 3. LOOP ==="
    AI WORKER NEW "audit", "{""workspace"":""work/rfc_audit"",""budget"":4000}"
    AI STATE SAVE "audit", "notes", "nota del run verificato"
    AI WORKER LOOP "VERIFY", 3 REQUIRE "LEN(RESULT$) > 5"
    IF AI.STATELOAD("audit", "verified") = "1" THEN PRINT "3.1 audit: verificato" ELSE PRINT "3.1 audit: non verificato"

    ' budget del run esaurito + policy error → stop ESPLICITO (nessun round tentato a vuoto)
    AI WORKER NEW "small", "{""workspace"":""work/rfc_small"",""budget"":8}"
    AI CONTEXT SET POLICY "worker:small", "error"
    AI STATE SAVE "small", "notes", "questa nota supera di molto il budget di otto caratteri"
    AI WORKER LOOP "VERIFY", 5
    IF INSTR(AI.STATELOAD("small", "last_error"), "run budget") > 0 THEN PRINT "3.2 budget-run: stop esplicito" ELSE PRINT "3.2 budget-run: non rilevato"

    PRINT "3.3 rounds= " + STR$(AI.WORKERROUNDS())
    IF AI.WORKERRESCUED() > 0 THEN PRINT "3.4 rescued: si" ELSE PRINT "3.4 rescued: no"
    IF AI.WORKERSPEND() > 0 THEN PRINT "3.5 spend: si" ELSE PRINT "3.5 spend: no"

    ' ══ 4. OBSERVE: ContextObject registry (FILE/LOG/SCREEN + letture) ══════════════
    PRINT "=== 4. OBSERVE ==="
    DIM fid AS DOUBLE
    AI OBSERVE FILE fixture INTO fid
    PRINT "4.1 file id= " + STR$(fid) + " hash= " + AI.OBSERVATIONHASH$(fid)
    PRINT "4.2 file path= " + AI.OBSERVATIONFILE$(fid)
    DIM lid AS DOUBLE
    AI OBSERVE LOG fixture INTO lid
    PRINT "4.3 log id= " + STR$(lid)
    PRINT "4.4 latest= " + AI.OBSERVATIONLATEST$()
    PRINT "4.5 count= " + STR$(AI.OBSERVATIONCOUNT())
    IF LEN(AI.OBSERVATION$(lid)) > 0 THEN PRINT "4.6 json: presente" ELSE PRINT "4.6 json: vuoto"
    IF LEN(AI.OBSERVATIONLIST$()) > 0 THEN PRINT "4.7 lista: presente" ELSE PRINT "4.7 lista: vuota"

    ' SCREEN: la cartella viene creata dal runtime; se la cattura non è concessa alla macchina
    ' l'errore è ESPLICITO e catturabile (mai silenzioso) → guardia ON ERROR per restare eseguibile.
    DIM cBefore AS DOUBLE
    DIM cAfter AS DOUBLE
    ON ERROR GOTO ScreenKo
    cBefore = AI.OBSERVATIONCOUNT()
    AI OBSERVE SCREEN "work/rfc_screen.png"
    cAfter = AI.OBSERVATIONCOUNT()
    IF cAfter = cBefore + 1 THEN PRINT "4.8 screen: registrata" ELSE PRINT "4.8 screen: non registrata"
    GOTO ScreenEnd
ScreenKo:
    PRINT "4.8 screen: cattura non disponibile (errore catturato)"
ScreenEnd:

    ' ══ 5. MANAGER LOOP sul task graph (P4) ═════════════════════════════════════════
    PRINT "=== 5. MANAGER LOOP ==="
    DIM tk AS DOUBLE
    tk = AI.TASKDEFINE("rfc_step1", "primo passo del piano")
    tk = AI.TASKDEFINE("rfc_step2", "secondo passo (dipende dal primo)")
    tk = AI.TASKDEPENDS("rfc_step2", "rfc_step1")
    AI WORKER NEW "manager", "{""workspace"":""work/rfc_manager"",""budget"":4000}"
    ' auditor che accetta la risposta del worker (in CI è l'errore esplicito del provider)
    AI WORKER MANAGE , 4 REQUIRE "LEN(RESULT$) > 5"
    PRINT "5.1 manage_exit= " + AI.STATELOAD("manager", "manage_exit")
    IF INSTR(AI.TASKSTATUS("rfc_step1"), "done") > 0 THEN PRINT "5.2 step1: done" ELSE PRINT "5.2 step1: no"
    IF INSTR(AI.TASKSTATUS("rfc_step2"), "done") > 0 THEN PRINT "5.3 step2: done" ELSE PRINT "5.3 step2: no"
    ' auditor severo → dissenso loggato (l'esito scartato non si perde: anti-anchoring D2)
    tk = AI.TASKDEFINE("rfc_step3", "passo con auditor severo")
    AI WORKER NEW "manager2", "{""workspace"":""work/rfc_manager2"",""budget"":4000}"
    AI WORKER MANAGE , 2 REQUIRE "LEN(RESULT$) > 500"
    PRINT "5.4 dissents= " + AI.STATELOAD("manager2", "dissents")
    PRINT "5.5 esito= " + AI.STATELOAD("manager2", "manage_exit")

    ' ══ 6. REPLAY AUTOMATICO degli eventi (E1) ═══════════════════════════════════════
    PRINT "=== 6. REPLAY AUTO ==="
    DIM rs AS DOUBLE
    rs = AI.REPLAYSTART("rfc_replay")
    rs = AI.REPLAYRECORD("1 + 1")
    rs = AI.REPLAYRECORD("LEN(""abc"")")
    DIM rk AS DOUBLE
    rk = AI.REPLAYRECORD("EVENTO_NON_VALIDO")
    rs = AI.REPLAYSTOP()
    ' riesecuzione AUTOMATICA: gli esiti per-evento vanno nella sessione "<s>#auto"
    AI REPLAY AUTO "rfc_replay"
    DIM esiti AS STRING
    esiti = AI.REPLAYGET("rfc_replay#auto")
    IF INSTR(esiti, "ok: 1 + 1") > 0 THEN PRINT "6.1 ok: evento eseguito" ELSE PRINT "6.1 ok: no"
    IF INSTR(esiti, "error: EVENTO_NON_VALIDO") > 0 THEN PRINT "6.2 error: evento tracciato" ELSE PRINT "6.2 error: no"

    PRINT "RUNTIME-FULL-COVERAGE-OK"
END SUB
