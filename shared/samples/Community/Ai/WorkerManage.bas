' WORKER-MANAGE — P4: MANAGER LOOP sul task graph (cicli manager→worker con auditor indipendente).
'
' Il manager prende dal task graph il prossimo task **lavorabile** (primo `pending` in ordine
' alfabetico con TUTTE le dipendenze `done`) e lo assegna al worker con l'**invocazione pulita**
' (vista curata sul workspace, budget CUMULATIVO del run). L'**auditor indipendente** (`REQUIRE`,
' stesso valutatore di `AI VERIFY … REQUIRE`) decide:
'   • soddisfatta → il task diventa `done` con quel risultato (visibile con `AI.TASKSTATUS`);
'   • rifiutata  → **DISSENSO loggato** (`state/dissent` + `state/dissents`): l'esito scartato resta
'     nella memoria del run e il ciclo successivo lo vede (anti-anchoring, D2) mentre il task resta
'     `pending` per un nuovo tentativo.
'
' Esiti in `state/manage_exit`: 1 fatto · 0 budget del run esaurito · -1 nessun worker ·
' -2 errore provider · -3 cicli esauriti con task pendenti · -4 deadlock di dipendenze.
' Copre: AI.TASKDEFINE / TASKDEPENDS / TASKSTATUS / TASKCLEAR · AI WORKER NEW/MANAGE ·
' AI.STATELOAD (manage_exit/dissents/verified) · AI.WORKERROUNDS.
' Marker finale: WORKER-MANAGE-OK
IMPORT AI

SUB Main()
    ' ── 1. piano di lavoro sul task graph: due task, il secondo dipende dal primo ────────
    DIM ok AS DOUBLE
    ok = AI.TASKDEFINE("analisi", "analizza i requisiti")
    ok = AI.TASKDEFINE("sintesi", "sintetizza la soluzione")
    ok = AI.TASKDEPENDS("sintesi", "analisi")
    PRINT "1. task graph: analisi → sintesi"

    ' ── 2. manager loop con auditor: il worker esegue, l'auditor giudica ─────────────────
    ' Senza provider LLM la risposta del worker è l'errore ESPLICITO; l'auditor qui la accetta
    ' (`LEN(RESULT$) > 5`) per restare deterministico in CI: è il comportamento reale del comando.
    AI WORKER NEW "mgr", "{""workspace"":""work/wman"",""budget"":4000}"
    AI STATE SAVE "mgr", "notes", "piano: analisi → sintesi"
    AI WORKER MANAGE , 4 REQUIRE "LEN(RESULT$) > 5"
    PRINT "2. manage_exit=" + AI.STATELOAD("mgr", "manage_exit")
    DIM st AS STRING
    st = AI.TASKSTATUS("analisi")
    IF INSTR(st, "done") > 0 THEN PRINT "3. analisi: done" ELSE PRINT "3. analisi: non done"
    st = AI.TASKSTATUS("sintesi")
    IF INSTR(st, "done") > 0 THEN PRINT "4. sintesi: done" ELSE PRINT "4. sintesi: non done"
    IF AI.STATELOAD("mgr", "verified") = "1" THEN PRINT "5. auditor: verificato" ELSE PRINT "5. auditor: non verificato"
    PRINT "6. rounds=" + STR$(AI.WORKERROUNDS())

    ' ── 3. auditor SEVERO: rifiuta la risposta → dissenso loggato, task ancora pending ───
    DIM t AS DOUBLE
    t = AI.TASKDEFINE("severo", "task con auditor severo")
    AI WORKER NEW "mgr2", "{""workspace"":""work/wman2"",""budget"":4000}"
    AI WORKER MANAGE , 2 REQUIRE "LEN(RESULT$) > 500"
    PRINT "7. dissents=" + AI.STATELOAD("mgr2", "dissents")
    st = AI.TASKSTATUS("severo")
    IF INSTR(st, "pending") > 0 THEN PRINT "8. severo: pending" ELSE PRINT "8. severo: non pending"
    PRINT "9. esito=" + AI.STATELOAD("mgr2", "manage_exit")

    ' ── 4. deadlock di dipendenze: esito esplicito -4, nessun loop infinito ──────────────
    DIM cl AS DOUBLE
    cl = AI.TASKCLEAR()
    ok = AI.TASKDEFINE("a", "task A")
    ok = AI.TASKDEFINE("b", "task B")
    ok = AI.TASKDEPENDS("a", "b")
    ok = AI.TASKDEPENDS("b", "a")
    AI WORKER NEW "mgr3", "{""workspace"":""work/wman3"",""budget"":4000}"
    AI WORKER MANAGE , 3
    PRINT "10. deadlock=" + AI.STATELOAD("mgr3", "manage_exit")

    PRINT "WORKER-MANAGE-OK"
END SUB
