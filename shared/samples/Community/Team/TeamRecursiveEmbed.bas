' ==============================================================================
' FILE: Team/TeamRecursiveEmbed.bas
' RecursiveMAS R2 — pseudo-latente via embedding (proxy 768-dim deterministico).
'   AI TEAM T SET LATENT embed   -> ogni risposta produce una fingerprint latente
'                                   (R_in residual + R_out) nel contesto dell'agente
'                                   successivo; nessun provider richiesto.
' Osservabilità: fingerprint salvata in Metadata["lastLatentFp"] e appesa al contesto.
' Benchmark token: AI.TEAMRUNTIMESTATS("T") stima tokens = caratteri/4 per round.
' ==============================================================================
IMPORT AI WITH ALIAS

SUB Main()
    AI AGENT CREATE Planner
    AI AGENT CREATE Solver
    AI TEAM CREATE Squadra
    AI TEAM Squadra ADD AGENT Planner
    AI TEAM Squadra ADD AGENT Solver
    AI TEAM Squadra SET STRATEGY sequential
    AI TEAM Squadra SET RECURSION 2
    AI TEAM Squadra SET LATENT embed
    AI TEAM Squadra SET LINK inner, "{""hidden"":64}"
    AI TEAM Squadra SET LINK outer, "{""hidden"":64}"

    PRINT "== RUN r=2 (embed) =="
    AI TEAM Squadra RUN "Qual è la capitale dell'Italia?"
    PRINT "run ok"

    PRINT "latent="; AI.TEAMLATENT("Squadra")

    DIM stats$ AS STRING
    stats$ = AI.TEAMRUNTIMESTATS("Squadra")
    PRINT "stats="; stats$

    PRINT "Completato."
END SUB
