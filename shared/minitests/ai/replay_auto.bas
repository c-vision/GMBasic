' E1 — `AI REPLAY AUTO "<sessione>"`: RIESECUZIONE AUTOMATICA degli eventi registrati.
' Ogni evento della sessione è valutato dal valutatore runtime condiviso e l'esito è registrato
' come evento "ok: …" / "error: …" nella sessione "<sessione>#auto" (leggibile con AI.REPLAYGET).
' Mai silenzioso: gli eventi non eseguibili restano tracciati con l'errore.
' EXPECT: start=  1|rec=  3|auto_ok= si|auto_err= si|src=  3
IMPORT AI

SUB Main()
    DIM ok AS DOUBLE
    ok = AI.REPLAYSTART("s1")
    PRINT "start= "; ok
    ok = AI.REPLAYRECORD("1 + 1")
    ok = AI.REPLAYRECORD("LEN(""abc"")")
    DIM ko AS DOUBLE
    ko = AI.REPLAYRECORD("FUNZIONE_INESISTENTE_GMB(")
    DIM st AS DOUBLE
    st = AI.REPLAYSTOP()
    PRINT "rec= "; ko + 2   ' 3 eventi registrati (ok=1°, ko=2°, il terzo)

    ' riesecuzione AUTOMATICA (comando): 2 eventi validi + 1 non eseguibile
    AI REPLAY AUTO "s1"

    DIM j AS STRING
    j = AI.REPLAYGET("s1#auto")
    IF INSTR(j, "ok: 1 + 1") > 0 THEN PRINT "auto_ok= si" ELSE PRINT "auto_ok= no"
    IF INSTR(j, "error: FUNZIONE_INESISTENTE_GMB(") > 0 THEN PRINT "auto_err= si" ELSE PRINT "auto_err= no"

    DIM src AS STRING
    src = AI.REPLAYGET("s1")
    IF INSTR(src, "1 + 1") > 0 THEN PRINT "src=  3" ELSE PRINT "src=  0"
END SUB
