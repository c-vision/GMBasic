' DecideLocalThinking.bas — il readout di AI DECIDE su un modello CON thinking (Qwen3, R1, ...).
'
' Il provider locale chiude il blocco `<think>` prima di leggere le opzioni: senza, la posizione
' letta aspetta `<think>` e le lettere A..P sono rumore (sempre A). Domande banali, risposta nota.
' Uso: DecideLocalThinking [cartella_modello_assoluta]   (percorso ASSOLUTO, es. /Users/me/ai/Models/Qwen3-8B-4bit)
' PARITY-EXCLUDE: richiede un modello reale con thinking.
IMPORT AI

SUB Main()
    DIM model$ AS STRING
    IF COMMANDCOUNT() >= 1 THEN model$ = COMMAND$(1)
    IF LEN(model$) = 0 THEN
        PRINT "uso: DecideLocalThinking <cartella_modello_ASSOLUTA>"
        EXIT SUB
    END IF

    AI REGISTER "dthink", "completion", model$, "{""engine"":""mlx"",""maxTokens"":""8"",""temperature"":""0""}"
    AI SET ACTIVE "completion", "dthink"

    DIM q1 AS DECISION
    DIM q2 AS DECISION
    DIM q3 AS DECISION
    AI DECIDE
        q1 AS CHOICE ("A", "B", "C")
    FROM "Which of these is a fruit? A) car B) apple C) chair"
    USING "local"
    END DECIDE
    AI DECIDE
        q2 AS CHOICE ("A", "B", "C", "D")
    FROM "What is the capital of France? A) Rome B) Berlin C) Madrid D) Paris"
    USING "local"
    END DECIDE
    AI DECIDE
        q3 AS CHOICE ("A", "B", "C")
    FROM "What is 2+2? A) 3 B) 5 C) 4"
    USING "local"
    END DECIDE

    DIM ok AS INTEGER
    ok = 0
    IF q1.VALUE = "B" THEN ok = ok + 1
    IF q2.VALUE = "D" THEN ok = ok + 1
    IF q3.VALUE = "C" THEN ok = ok + 1
    PRINT "q1="; q1.VALUE; " q2="; q2.VALUE; " q3="; q3.VALUE
    PRINT "corrette="; ok; "/3"
    IF ok = 3 THEN PRINT "DECIDE-THINKING PASS" ELSE PRINT "DECIDE-THINKING FAIL"
END SUB
