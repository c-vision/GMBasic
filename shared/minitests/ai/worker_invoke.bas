' P3 (C1) — `AI WORKER "<TASK>"`: INVOCAZIONE PULITA del worker corrente.
' La vista della chiamata è SOLO la memoria esterna del run (workspace: task/plan/notes/solution/
' state) normalizzata dal Context Engine — mai la chat history (context isolation).
' Due esiti deterministici senza provider: errore ESPLICITO (mai un esito finto) e rifiuto per
' budget con policy `error` (mai un taglio silenzioso). Contatori run-level + last_error osservabili.
' EXPECT: rounds= 1|resc= 1|spent= si|task=si|state=si|err=esplicito|r2= 2|resc2= 2|budget= rifiutata
IMPORT AI

SUB Main()
    ' ── 1) invocazione pulita: senza provider → errore esplicito + rescue ────────────
    AI WORKER NEW "w1", "{""workspace"":""bin/wi"",""budget"":1000}"
    AI STATE SAVE "w1", "notes", "greedy algorithm"
    AI WORKER "ANALYZE"
    PRINT "rounds="; AI.WORKERROUNDS()
    PRINT "resc="; AI.WORKERRESCUED()
    IF AI.WORKERSPEND() > 0 THEN PRINT "spent= si" ELSE PRINT "spent= no"
    DIM v AS STRING
    v = AI.CONTEXTVIEW$("worker:w1")
    IF INSTR(v, "TASK: ANALYZE") > 0 THEN PRINT "task=si" ELSE PRINT "task=no"
    IF INSTR(v, "greedy algorithm") > 0 THEN PRINT "state=si" ELSE PRINT "state=no"
    IF INSTR(AI.STATELOAD("w1", "last_error"), "provider") > 0 THEN PRINT "err=esplicito" ELSE PRINT "err=assente"

    ' ── 2) budget del worker + policy error → invocazione RIFIUTATA (rescue) ────────
    AI WORKER NEW "w2", "{""workspace"":""bin/wi2"",""budget"":8}"
    AI CONTEXT SET POLICY "worker:w2", "error"
    AI STATE SAVE "w2", "notes", "una nota abbastanza lunga da superare il budget del worker"
    AI WORKER "VERIFY"
    PRINT "r2="; AI.WORKERROUNDS()
    PRINT "resc2="; AI.WORKERRESCUED()
    IF INSTR(AI.STATELOAD("w2", "last_error"), "overflow") > 0 THEN PRINT "budget= rifiutata" ELSE PRINT "budget= ?"
END SUB
