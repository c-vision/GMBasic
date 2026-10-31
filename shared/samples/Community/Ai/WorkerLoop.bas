' WORKER-LOOP — P3/P4 (assi C + D1): worker ISOLATO, invocazione pulita, loop multi-round.
'
' Un worker è un run isolato con memoria esterna su filesystem (workspace). La chiamata NON vede
' la conversazione: vede SOLO `TASK` + `plan.md` + `notes.md` + `solution` + `best` + `state/*`
' del workspace, normalizzati dal Context Engine (budget + policy). `AI WORKER LOOP` ripete la
' invocazione aggiornando la memoria ad ogni round (best-so-far) e consuma un budget CUMULATIVO
' del run: quando finisce, il loop si FERMA con un esito esplicito.
'
' Copre TUTTA la superficie del filone: AI WORKER NEW/RUN/"<TASK>"/LOOP (anche col REQUIRE
' dell'auditor indipendente) · AI STATE SAVE/LOAD · AI CONTEXT SET BUDGET/POLICY/VIEW ·
' AI.CONTEXTUSED/BUDGET/OVERFLOW/VIEW$ · AI.WORKERROUNDS/RESCUED/SPEND.
'
' Senza provider LLM attivo (`AI SET ACTIVE %CHAT, …`) l'esito è l'errore esplicito
' "nessun provider LLM configurato": è l'esito deterministico atteso in CI (mai un risultato
' finto). Con un provider reale il loop esegue i round e promuove il best-so-far in `best`.
' Marker finale: WORKER-LOOP-OK
IMPORT AI

SUB Main()
    ' ── 1) worker con workspace dedicato e budget del RUN ─────────────────────────
    AI WORKER NEW "wdemo", "{""workspace"":""work/wdemo"",""budget"":4000}"
    AI STATE SAVE "wdemo", "notes", "greedy algorithm: scambio locale ottimo"
    AI STATE SAVE "wdemo", "plan", "1) leggere  2) risolvere  3) verificare"
    PRINT "new= ok"
    PRINT "stato= "; AI.STATELOAD("wdemo", "plan")

    ' ── 2) invocazione singola: vista curata = memoria esterna del run ────────────
    AI WORKER "ANALYZE"
    DIM v AS STRING
    v = AI.CONTEXTVIEW$("worker:wdemo")
    IF INSTR(v, "TASK: ANALYZE") > 0 THEN PRINT "vista:task" ELSE PRINT "vista:senza-task"
    IF INSTR(v, "greedy algorithm") > 0 THEN PRINT "vista:memoria-run" ELSE PRINT "vista:senza-memoria"
    IF INSTR(v, "user:") = 0 THEN PRINT "isolamento: ok" ELSE PRINT "isolamento: no"
    PRINT "used= "; AI.CONTEXTUSED("worker:wdemo")
    PRINT "budget= "; AI.CONTEXTBUDGET("worker:wdemo")
    PRINT "overflow= "; AI.CONTEXTOVERFLOW("worker:wdemo")
    DIM err AS STRING
    err = AI.STATELOAD("wdemo", "last_error")
    IF INSTR(err, "provider") > 0 THEN PRINT "provider: assente (errore esplicito)" ELSE PRINT "provider: attivo"

    ' ── 3) round di lifecycle sul worker indicato ─────────────────────────────────
    AI WORKER RUN "wdemo"
    PRINT "run= ok"

    ' ── 4) LOOP multi-round con budget CUMULATIVO del run ─────────────────────────
    AI WORKER LOOP "IMPLEMENT", 3
    PRINT "rounds= "; AI.WORKERROUNDS()
    IF AI.WORKERRESCUED() > 0 THEN PRINT "rescued: si" ELSE PRINT "rescued: no"
    IF AI.WORKERSPEND() > 0 THEN PRINT "spend: si" ELSE PRINT "spend: no"

    ' ── 5) budget del run esaurito + policy error → stop ESPLICITO (mai silenzioso)
    AI WORKER NEW "wsmall", "{""workspace"":""work/wsmall"",""budget"":8}"
    AI CONTEXT SET POLICY "worker:wsmall", "error"
    AI CONTEXT SET BUDGET "worker:wsmall", 8
    AI STATE SAVE "wsmall", "notes", "questa nota supera di molto il budget di otto caratteri"
    AI WORKER LOOP "VERIFY", 5
    DIM err2 AS STRING
    err2 = AI.STATELOAD("wsmall", "last_error")
    IF INSTR(err2, "run budget") > 0 OR INSTR(err2, "overflow") > 0 THEN
        PRINT "budget-run: stop esplicito"
    ELSE
        PRINT "budget-run: non rilevato"
    END IF

    ' ── 6) LOOP VERIFICATO: auditor indipendente (REQUIRE con RESULT$ = risposta) ─────
    AI WORKER NEW "waudit", "{""workspace"":""work/waudit"",""budget"":4000}"
    AI STATE SAVE "waudit", "notes", "verifica indipendente del round"
    AI WORKER LOOP "VERIFY", 3 REQUIRE "LEN(RESULT$) > 5"
    IF AI.STATELOAD("waudit", "verified") = "1" THEN
        PRINT "audit: verificato (state/verified=1)"
    ELSE
        PRINT "audit: non verificato"
    END IF

    ' audit RIFIUTATO dalla risposta → stop esplicito, nessun rework a vuoto
    AI WORKER NEW "waudit2", "{""workspace"":""work/waudit2"",""budget"":4000}"
    AI WORKER LOOP "VERIFY", 3 REQUIRE "LEN(RESULT$) > 500"
    IF INSTR(AI.STATELOAD("waudit2", "last_error"), "provider") > 0 THEN
        PRINT "audit: rifiutato (stop esplicito)"
    ELSE
        PRINT "audit: esito inatteso"
    END IF

    ' ── 7) VIEW manuale (normalizzata subito da policy/budget) ────────────────────
    AI CONTEXT SET POLICY "worker:wdemo", "truncate"
    AI CONTEXT VIEW "worker:wdemo", "vista manuale di prova"
    IF AI.CONTEXTVIEW$("worker:wdemo") = "vista manuale di prova" THEN PRINT "view: ok" ELSE PRINT "view: troncata"

    PRINT "WORKER-LOOP-OK"
END SUB
