' PARITY-EXCLUDE: parità greedy su modello LOCALE pesante (2 generazioni).
' FILE: AiLLM/ModelSurgeryRoundTripParity.bas
' parità IDS (§6 crit.4): un ciclo LOSSLESS (SAVE round-trip) deve preservare la generazione greedy.
'   ORIGINALE (work/dsv2l_mlx) vs ROUND-TRIP (work/dsv2l_emit_mlx) -> output IDENTICI attesi.
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
    PRINT "=== PARITÀ IDS (§6 crit.4) — originale vs round-trip lossless ==="
    IF NOT IO.FILEEXISTS("work/dsv2l_mlx/config.json") THEN
        PRINT "assente work/dsv2l_mlx"
        END
    END IF
    IF NOT IO.FILEEXISTS("work/dsv2l_emit_mlx/config.json") THEN
        PRINT "assente work/dsv2l_emit_mlx"
        END
    END IF
    Poni("dsv2l")
    r1 = Chiedi()
    PRINT "ORIGINALE : "; r1
    Poni("dsv2l_emit")
    r2 = Chiedi()
    PRINT "ROUND-TRIP: "; r2
    IF r1 = r2 THEN
        PRINT "PARITÀ IDS: OK — round-trip lossless preserva la generazione."
    ELSE
        PRINT "PARITÀ IDS: DIVERGENZA — il round-trip non è lossless."
    END IF
END SUB
