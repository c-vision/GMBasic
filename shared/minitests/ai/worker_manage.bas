' P4 — `AI WORKER MANAGE [, cycles] [REQUIRE "<expr>"]`: MANAGER LOOP sul task graph.
' Il manager prende il prossimo task lavorabile (primo `pending` in ordine alfabetico con dipendenze
' `done`), lo assegna al worker con l'invocazione pulita (vista curata + budget cumulativo del run) e
' l'auditor indipendente (REQUIRE con RESULT$ = risposta) decide:
'   • soddisfatta → task `done` con quel risultato (osservabile con AI.TASKSTATUS);
'   • rifiutata  → DISSENSO loggato (state/dissent + conteggio state/dissents) e task ancora pending.
' Esiti in state/manage_exit: 1 fatto · 0 budget del run · -1 nessun worker · -2 errore provider ·
' -3 cicli esauriti con task pendenti · -4 deadlock di dipendenze.
' Senza provider la risposta del worker è l'errore ESPLICITO: l'audit `LEN(RESULT$) > 5` lo accetta
' (caso deterministico), un audit che lo rifiuta produce il dissenso e poi lo stop -2.
' Scoping su `AI.PLAN*`: `AI WORKER MANAGE "<piano>"` lavora SOLO sui task del piano e pretende il
' piano APPROVATO (`AI PLAN APPROVE`); un piano in bozza → esito esplicito -5.
' EXPECT: notasks= 1|t1_done= done|t1_result= si|dep2_done= done|dissents= 1|t3_pending= pending|err= provider|exit=-2|nonappr= -5|plan_only= done|fuori= pending|deadlock= -4
IMPORT AI

SUB Main()
    ' ── A) nessun task nel grafo → niente da fare (esito 1, nessuna invocazione) ────────────
    AI WORKER NEW "m0", "{""workspace"":""bin/wm0"",""budget"":4000}"
    AI WORKER MANAGE , 2
    PRINT "notasks= "; AI.STATELOAD("m0", "manage_exit")

    ' ── B) un task, audit che accetta la risposta → task `done` ─────────────────────────────
    DIM t1 AS DOUBLE
    DIM t2 AS DOUBLE
    t1 = AI.TASKDEFINE("t1", "risolvi il problema")
    AI WORKER NEW "m1", "{""workspace"":""bin/wm1"",""budget"":4000}"
    AI WORKER MANAGE , 2 REQUIRE "LEN(RESULT$) > 5"
    DIM s1 AS STRING
    s1 = AI.TASKSTATUS("t1")
    IF INSTR(s1, "done") > 0 THEN PRINT "t1_done= done" ELSE PRINT "t1_done= no"
    IF INSTR(s1, "provider") > 0 THEN PRINT "t1_result= si" ELSE PRINT "t1_result= no"

    ' ── C) task con DIPENDENZA: il manager lo esegue solo dopo il primo ─────────────────────
    t2 = AI.TASKDEFINE("t2", "sintetizza il risultato")
    DIM dep AS DOUBLE
    dep = AI.TASKDEPENDS("t2", "t1")
    AI WORKER NEW "m2", "{""workspace"":""bin/wm2"",""budget"":4000}"
    AI WORKER MANAGE , 3 REQUIRE "LEN(RESULT$) > 5"
    DIM s2 AS STRING
    s2 = AI.TASKSTATUS("t2")
    IF INSTR(s2, "done") > 0 THEN PRINT "dep2_done= done" ELSE PRINT "dep2_done= no"

    ' ── D) DISSENSO: l'audit rifiuta → l'esito scartato è loggato, poi stop -2 (provider) ──
    DIM t3 AS DOUBLE
    t3 = AI.TASKDEFINE("t3", "task con audit severo")
    AI WORKER NEW "m3", "{""workspace"":""bin/wm3"",""budget"":4000}"
    AI WORKER MANAGE , 2 REQUIRE "LEN(RESULT$) > 500"
    PRINT "dissents= "; AI.STATELOAD("m3", "dissents")
    DIM s3 AS STRING
    s3 = AI.TASKSTATUS("t3")
    IF INSTR(s3, "pending") > 0 THEN PRINT "t3_pending= pending" ELSE PRINT "t3_pending= no"
    IF INSTR(AI.STATELOAD("m3", "last_error"), "provider") > 0 THEN PRINT "err= provider" ELSE PRINT "err= altro"
    PRINT "exit="; AI.STATELOAD("m3", "manage_exit")

    ' ── E) SCOPING su PIANO: piano in bozza → -5; approvato → solo i task del piano ────────
    DIM pd AS DOUBLE
    DIM pa AS DOUBLE
    pd = AI.PLANDEFINE("p1", "obiettivo del piano")
    DIM pt AS DOUBLE
    pt = AI.TASKDEFINE("pt1", "task del piano")
    pa = AI.PLANADD("p1", "pt1")
    AI WORKER NEW "mp1", "{""workspace"":""bin/wmp1"",""budget"":4000}"
    AI WORKER MANAGE "p1", 2
    PRINT "nonappr= "; AI.STATELOAD("mp1", "manage_exit")
    pa = AI.PLANAPPROVE("p1")
    DIM out1 AS DOUBLE
    out1 = AI.TASKDEFINE("fuori", "task fuori dal piano")
    AI WORKER NEW "mp2", "{""workspace"":""bin/wmp2"",""budget"":4000}"
    AI WORKER MANAGE "p1", 3 REQUIRE "LEN(RESULT$) > 5"
    DIM sp AS STRING
    sp = AI.TASKSTATUS("pt1")
    IF INSTR(sp, "done") > 0 THEN PRINT "plan_only= done" ELSE PRINT "plan_only= no"
    sp = AI.TASKSTATUS("fuori")
    IF INSTR(sp, "pending") > 0 THEN PRINT "fuori= pending" ELSE PRINT "fuori= no"

    ' ── F) DEADLOCK: dipendenze incrociate → esito esplicito -4 (nessun loop infinito) ─────
    ' Il grafo va azzerato (AI.TASKCLEAR) altrimenti restano i task dei casi precedenti e il
    ' manager ne troverebbe uno lavorabile prima di accorgersi del ciclo.
    DIM cleared AS DOUBLE
    cleared = AI.TASKCLEAR()
    DIM da AS DOUBLE
    DIM db AS DOUBLE
    da = AI.TASKDEFINE("da", "task A")
    db = AI.TASKDEFINE("db", "task B")
    da = AI.TASKDEPENDS("da", "db")
    db = AI.TASKDEPENDS("db", "da")
    AI WORKER NEW "m4", "{""workspace"":""bin/wm4"",""budget"":4000}"
    AI WORKER MANAGE , 3
    PRINT "deadlock= "; AI.STATELOAD("m4", "manage_exit")
END SUB
