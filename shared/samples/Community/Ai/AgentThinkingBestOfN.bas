' FILE: Ai/AgentThinkingBestOfN.bas
' =================================================================================================
'   T1.1 Thinking Mode / Best-of-N (Reasoning Regimes, AI-Research 2026-09) — superficie BASIC:
'     AI AGENT SET <nome>, thinking, ON|OFF      (regime "pensa prima di rispondere")
'     AI AGENT SET <nome>, samples, <n>          (Best-of-N: ASK senza tool campiona N volte e
'                                                 sceglie il vincitore col voto di maggioranza)
'     AI AGENT SET <nome>, thoughttokens, <k>    (token di pensiero per output)
'   Lettura: AI.AGENTGET(<nome>, <proprieta>). Con samples=1 (default) l'ASK resta a singolo
'   campione. Nota: la semantica di campionamento si osserva con un LLM (temperatura); qui si
'   verifica la CONFIGURAZIONE, deterministico e senza LLM.
' =================================================================================================
IMPORT AI WITH ALIAS

SUB Main()
    AI AGENT CREATE "Ragionatore"

    AI AGENT SET "Ragionatore" thinking ON
    AI AGENT SET "Ragionatore" samples 3
    AI AGENT SET "Ragionatore" thoughttokens 16

    DIM t AS STRING
    t = AI.AGENTGET("Ragionatore", "thinking")
    DIM s AS STRING
    s = AI.AGENTGET("Ragionatore", "samples")
    DIM k AS STRING
    k = AI.AGENTGET("Ragionatore", "thoughttokens")

    IF t = "ON" AND s = "3" AND k = "16" THEN
        PRINT "bofn-thinking-on"
        PRINT "bofn-samples:"; s
        PRINT "bofn-thoughttokens:"; k
    ELSE
        PRINT "bofn-FAIL t="; t; " s="; s; " k="; k
    END IF

    ' Off è il default di sicurezza: riporta a regime singolo campione.
    AI AGENT SET "Ragionatore" samples 1
    DIM s2 AS STRING
    s2 = AI.AGENTGET("Ragionatore", "samples")
    IF s2 = "1" THEN PRINT "bofn-reset-ok" ELSE PRINT "bofn-FAIL-reset:"; s2

    AI AGENT DELETE "Ragionatore"
    PRINT "bofn-done"
END SUB
