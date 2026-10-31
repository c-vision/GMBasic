' P4 residue (assi C + D1) — `AI WORKER LOOP "<TASK>"[, rounds]`: LOOP ISOLATO multi-round.
' Ogni round è una invocazione pulita con la vista curata AGGIORNATA dal round precedente
' (memoria esterna del run + BEST) e con budget CUMULATIVO del run (il budget del worker è il
' tetto dell'intero run, non della singola vista).
' Due esiti deterministici senza provider: stop esplicito al primo round con errore provider
' (-2, rescued, last_error) e stop per budget del run esaurito (0).
' EXPECT: r1= 1|resc1= 1|err1=provider|spent1= si|best1= no|r2= 1|resc2= 2|err2=run-budget|rounds-decl= 3
IMPORT AI

SUB Main()
    ' ── 1) loop con provider assente: si ferma al PRIMO round con errore esplicito ────
    AI WORKER NEW "wl1", "{""workspace"":""bin/wl1"",""budget"":4000}"
    AI STATE SAVE "wl1", "notes", "greedy algorithm: scambio locale ottimo"
    AI WORKER LOOP "ANALYZE", 3
    PRINT "r1="; AI.WORKERROUNDS()
    PRINT "resc1="; AI.WORKERRESCUED()
    IF INSTR(AI.STATELOAD("wl1", "last_error"), "provider") > 0 THEN PRINT "err1=provider" ELSE PRINT "err1=altro"
    IF AI.WORKERSPEND() > 0 THEN PRINT "spent1= si" ELSE PRINT "spent1= no"
    ' il best-so-far non viene promosso quando la soluzione è un errore
    IF AI.STATELOAD("wl1", "best") = "" THEN PRINT "best1= no" ELSE PRINT "best1= si"

    ' ── 2) budget CUMULATIVO del run esaurito → stop esplicito (policy error) ────────
    AI WORKER NEW "wl2", "{""workspace"":""bin/wl2"",""budget"":8}"
    AI CONTEXT SET POLICY "worker:wl2", "error"
    AI STATE SAVE "wl2", "notes", "una nota abbastanza lunga da superare il budget del run"
    AI WORKER LOOP "VERIFY", 5
    PRINT "r2="; AI.WORKERROUNDS()
    PRINT "resc2="; AI.WORKERRESCUED()
    IF INSTR(AI.STATELOAD("wl2", "last_error"), "run budget") > 0 THEN PRINT "err2=run-budget" ELSE PRINT "err2=altro"

    ' il comando è dichiarato (parser/4 backend) e il conteggio round è un'espressione
    AI WORKER LOOP "ANALYZE", 1 + 2
    PRINT "rounds-decl= 3"
END SUB
