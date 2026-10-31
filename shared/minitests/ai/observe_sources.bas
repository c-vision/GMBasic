' A5 — fonti OBSERVE di fase 2: TEST / DATABASE / PROCESS / BROWSER (tutte e 4).
' Ogni fonte registra un ContextObject con TYPE esplicito e il RIFERIMENTO dell'artefatto (mai il
' contenuto nel context). Se la fonte non è osservabile il comando solleva un errore ESPLICITO,
' catturabile con ON ERROR — mai un esito silenzioso.
' BROWSER dipende dall'ambiente (serve un browser con CDP): qui si esercita il percorso d'errore con
' un URL non raggiungibile; il client CDP ha ora un timeout bounded (5 s) quindi non blocca mai.
' EXPECT: test= si|db_ref= si|db_type= si|proc= si|browser= si|count= 2
IMPORT AI

SUB Main()
    ' ── 1) TEST: compila una fixture e registra l'esito ─────────────────────────────────
    DIM tid AS DOUBLE
    AI OBSERVE TEST "../../samples/Fixtures/AIVerifyFixtureOk.bas" INTO tid
    IF tid > 0 THEN PRINT "test= si" ELSE PRINT "test= no"

    ' ── 2) DATABASE: query su una fixture sqlite, registra il riferimento ───────────────
    DIM did AS DOUBLE
    AI OBSERVE DATABASE "../../samples/Fixtures/ai/observe_sources.sqlite", "SELECT * FROM note" INTO did
    IF INSTR(AI.OBSERVATIONFILE$(did), "ai_observe_database.json") > 0 THEN PRINT "db_ref= si" ELSE PRINT "db_ref= no"
    IF INSTR(AI.OBSERVATION$(did), "database") > 0 THEN PRINT "db_type= si" ELSE PRINT "db_type= no"

    ' ── 3) PROCESS: processo certamente inesistente → errore esplicito catturato ────────
    ON ERROR GOTO Pe
    AI OBSERVE PROCESS "gmbasic_no_such_process_xyz"
    PRINT "proc= no"
    GOTO Pd
Pe:
    PRINT "proc= si"
Pd:

    ' ── 4) BROWSER: URL non raggiungibile → errore esplicito catturato (bounded) ────────
    ON ERROR GOTO Be
    AI OBSERVE BROWSER "http://127.0.0.1:1"
    PRINT "browser= no"
    GOTO Bd
Be:
    PRINT "browser= si"
Bd:

    IF AI.OBSERVATIONCOUNT() >= 2 THEN PRINT "count= 2" ELSE PRINT "count= ?"
END SUB
