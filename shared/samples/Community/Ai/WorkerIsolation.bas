' WORKER-ISOLATION — P3 (asse C): `AI WORKER "<TASK>"` = INVOCAZIONE PULITA.
'
' Isola il contesto di una chiamata: la vista usata NON è la chat history ma la SOLO memoria
' esterna del run, cioè il workspace del worker (state salvato con AI STATE SAVE, più
' plan.md/notes.md/solution se presenti), normalizzata dal Context Engine (budget del worker +
' policy). Così due worker diversi non si vedono mai il contesto a vicenda.
'
' Copre: AI WORKER NEW (opts workspace/budget) · AI STATE SAVE · AI WORKER "<TASK>" (invocazione)
' · AI.CONTEXTVIEW$ (vista realmente inviata) · AI.CONTEXT SET POLICY worker:<nome> ·
' AI.WORKERROUNDS/WORKERRESCUED/WORKERSPEND · AI.STATELOAD (esito e last_error).
'
' Senza un provider LLM attivo (`AI SET ACTIVE %CHAT, ...`) l'invocazione NON è silenziosa:
' risponde con l'errore esplicito "nessun provider LLM configurato" e lo registra come rescue —
' è esattamente l'esito atteso in CI, deterministico su tutte e 5 le gambe.
' Marker finale: WORKER-ISOLATION-OK
IMPORT AI

SUB Main()
    ' ── 1) worker con workspace dedicato e budget ────────────────────────────────
    AI WORKER NEW "wi_demo", "{""workspace"":""work/wi_demo"",""budget"":4000}"
    AI STATE SAVE "wi_demo", "notes", "greedy algorithm: scambio locale ottimo"
    PRINT "new= ok"

    ' ── 2) invocazione pulita: la vista è il run, non la conversazione ───────────
    AI WORKER "ANALYZE"
    DIM v AS STRING
    v = AI.CONTEXTVIEW$("worker:wi_demo")
    IF INSTR(v, "TASK: ANALYZE") > 0 THEN PRINT "vista:task" ELSE PRINT "vista:senza-task"
    IF INSTR(v, "greedy algorithm") > 0 THEN PRINT "vista:memoria-run" ELSE PRINT "vista:senza-memoria"
    IF INSTR(v, "TASK: ANALYZE") > 0 AND INSTR(v, "user:") = 0 THEN PRINT "isolamento: ok" ELSE PRINT "isolamento: no"

    ' esito esplicito (in CI senza provider: errore registrato, mai un risultato finto)
    DIM err AS STRING
    err = AI.STATELOAD("wi_demo", "last_error")
    IF INSTR(err, "provider") > 0 THEN PRINT "provider: assente (errore esplicito)" ELSE PRINT "provider: attivo"
    IF AI.WORKERRESCUED() > 0 THEN PRINT "rescued: si" ELSE PRINT "rescued: no"
    IF AI.WORKERSPEND() > 0 THEN PRINT "spend: si" ELSE PRINT "spend: no"

    ' ── 3) budget del worker + policy error → invocazione RIFIUTATA (mai taglio silenzioso)
    AI WORKER NEW "wi_small", "{""workspace"":""work/wi_small"",""budget"":8}"
    AI CONTEXT SET POLICY "worker:wi_small", "error"
    AI STATE SAVE "wi_small", "notes", "questa nota supera di molto il budget di otto caratteri"
    AI WORKER "VERIFY"
    DIM err2 AS STRING
    err2 = AI.STATELOAD("wi_small", "last_error")
    IF INSTR(err2, "overflow") > 0 THEN PRINT "budget: rifiutata" ELSE PRINT "budget: non-rilevato"
    PRINT "rounds= "; AI.WORKERROUNDS()

    PRINT "WORKER-ISOLATION-OK"
END SUB
