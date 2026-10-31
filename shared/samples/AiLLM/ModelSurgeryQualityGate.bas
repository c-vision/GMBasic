' PARITY-EXCLUDE: quality gate su modello LOCALE pesante (2 generazioni).
' FILE: AiLLM/ModelSurgeryQualityGate.bas
' =====================================================================================
' QUALITY GATE (task-level, §4.5 I6 / §4.1 reject): la stessa domanda su ORIGINALE e
' TRASFORMATO; se la risposta diverge -> la trasformazione ha degradato il task -> REJECT/flag.
'   1. genera su work/dsv2l_mlx        (ORIGINALE)
'   2. genera su work/dsv2l_p48_mlx    (POTATO 64->48)
'   3. confronto -> ACCEPT (identico) / REJECT-flag (diverso)
' Prerequisiti: work/dsv2l_mlx, work/dsv2l_p48_mlx.
' =====================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Poni(nome$)
    AI REGISTER nome$, "completion", "work/" + nome$ + "_mlx", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""48"",""temperature"":""0"",""contextSize"":""2048"",""eosToken"":""<｜end▁of▁sentence｜>""}"
    AI SET ACTIVE "completion", nome$
END SUB

FUNCTION Chiedi() AS STRING
    AI AGENT CREATE "ag"
    AI AGENT SET "ag", maxtokens, "48"
    DIM r AS STRING
    r = AI.AGENTASKSYNC("ag", "Rispondi in una riga in italiano: qual e' la capitale dell'Italia?")
    AI AGENT DELETE "ag"
    Chiedi = r
END FUNCTION

SUB Main()
    DIM r1 AS STRING
    DIM r2 AS STRING
    PRINT "=== QUALITY GATE (task-level §4.5 I6) — originale vs potatato 64->48 ==="
    IF NOT IO.FILEEXISTS("work/dsv2l_mlx/config.json") THEN PRINT "assente work/dsv2l_mlx" : END
    IF NOT IO.FILEEXISTS("work/dsv2l_p48_mlx/config.json") THEN PRINT "assente work/dsv2l_p48_mlx" : END

    Poni("dsv2l")
    r1 = Chiedi()
    PRINT "ORIGINALE: "; r1

    Poni("dsv2l_p48")
    r2 = Chiedi()
    PRINT "POTATO   : "; r2

    IF r1 = r2 THEN
        PRINT "GATE task: ACCEPT — output identico."
    ELSE
        PRINT "GATE task: REJECT-FLAG — output divergente (possibile degrado)."
    END IF

    ' GATE NLL (quantitativo, automatico): ppl del potato vs originale -> ACCEPT/REJECT-FLAG.
    PRINT "GATE NLL (quality budget I6):"
    TRAIN EVAL "work/dsv2l_p48_mlx", "La capitale dell'Italia e' Roma.", "work/q_gate.txt", "work/dsv2l_mlx"
    LLM TENSOR LOGITS "work/dsv2l_mlx", "La capitale dell'Italia e'"
    PRINT "  verdetto nel file work/q_gate.txt (ppl potato vs originale)."
END SUB
