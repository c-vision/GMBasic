' DecideVerbalizer.bas — provider %DECIDE distinguibile: kind "verbalizer".
'
' Lo stesso modello locale ha due decisori registrati con nome proprio:
'   "jev"    verbalizer con la config del modello (decide=on di default);
'   "jevoff" verbalizer con "decide":"off" nella SUA config → la decisione e' disabilitata.
' La config del provider %DECIDE (decide / decideTemplate / decideCalibration) sovrascrive quella
' del provider completion solo per le chiamate fatte con quel decisore (USING "<nome>" oppure
' %DECIDE attivo). Nessun comando nuovo: e' solo il valore di kind in AI REGISTER.
' Uso: DecideVerbalizer [cartella_modello_assoluta]   (es. /Users/me/ai/Models/Llama-3.2-1B-Instruct-4bit)
' PARITY-EXCLUDE: richiede un modello reale.
IMPORT AI

SUB Main()
    DIM model$ AS STRING
    IF COMMANDCOUNT() >= 1 THEN model$ = COMMAND$(1)
    IF LEN(model$) = 0 THEN
        PRINT "uso: DecideVerbalizer <cartella_modello_ASSOLUTA>"
        EXIT SUB
    END IF

    AI REGISTER "dverb", "completion", model$, "{""engine"":""mlx"",""maxTokens"":""8"",""temperature"":""0""}"
    AI SET ACTIVE "completion", "dverb"

    AI REGISTER "jev", %DECIDE, "verbalizer", "{""decide"":""on""}"
    AI REGISTER "jevoff", %DECIDE, "verbalizer", "{""decide"":""off""}"

    DIM viaName AS DECISION
    AI DECIDE
        viaName AS CHOICE ("A", "B", "C")
    FROM "Which of these is a fruit? A) car B) apple C) chair"
    USING "jev"
    END DECIDE

    ' decide=off: forma stringa, cosi' l'esito e' il JSON d'errore su tutti i backend (la forma tipata lo solleva).
    DIM viaOff AS STRING
    AI DECIDE "Which of these is a fruit? A) car B) apple C) chair", "CHOICE:A|B|C" USING "jevoff" INTO viaOff

    AI SET ACTIVE %DECIDE, "jev"
    DIM viaActive AS DECISION
    AI DECIDE
        viaActive AS CHOICE ("A", "B", "C")
    FROM "Which of these is a fruit? A) car B) apple C) chair"
    END DECIDE

    DIM viaLocal AS DECISION
    AI DECIDE
        viaLocal AS CHOICE ("A", "B", "C")
    FROM "Which of these is a fruit? A) car B) apple C) chair"
    USING "local"
    END DECIDE

    PRINT "jev="; viaName.VALUE; " jevoff="; LEFT$(viaOff, 24); " attivo="; viaActive.VALUE; " local="; viaLocal.VALUE
    DIM ok AS INTEGER
    ok = 0
    IF LEN(viaName.VALUE) > 0 THEN ok = ok + 1
    IF INSTR(viaOff, "decide=off") > 0 THEN ok = ok + 1
    IF viaActive.VALUE = viaName.VALUE THEN ok = ok + 1
    IF LEN(viaLocal.VALUE) > 0 THEN ok = ok + 1
    PRINT "controlli="; ok; "/4"
    IF ok = 4 THEN PRINT "DECIDE-VERBALIZER PASS" ELSE PRINT "DECIDE-VERBALIZER FAIL"
END SUB
