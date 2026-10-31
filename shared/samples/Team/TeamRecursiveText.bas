' ==============================================================================
' FILE: Team/TeamRecursiveText.bas
' RecursiveMAS R1/R6 — loop ricorsivo TESTUALE (baseline Recursive-TextMAS).
'   AI TEAM T SET RECURSION n      -> r=1..8 round, closure A_N -> A_1
'   AI TEAM T SET LATENT text      -> modalità di default (nessun tag latente)
' Metrica per round: AI.TEAMRUNTIMESTATS("T") -> JSON {runs:[{round,timeMs,tokens,asks}]}
' ==============================================================================
IMPORT AI WITH ALIAS

SUB Main()
    AI AGENT CREATE Planner
    AI AGENT CREATE Critic
    AI AGENT CREATE Solver
    AI TEAM CREATE Squadra
    AI TEAM Squadra ADD AGENT Planner
    AI TEAM Squadra ADD AGENT Critic
    AI TEAM Squadra ADD AGENT Solver
    AI TEAM Squadra SET STRATEGY sequential
    AI TEAM Squadra SET RECURSION 3
    AI TEAM Squadra SET LATENT text

    PRINT "== RUN r=3 (text) =="
    AI TEAM Squadra RUN "Quanti secondi in 3 ore?"
    PRINT "run ok"

    PRINT "recursion="; AI.TEAMRECURSION("Squadra")
    PRINT "latent="; AI.TEAMLATENT("Squadra")

    DIM stats$ AS STRING
    stats$ = AI.TEAMRUNTIMESTATS("Squadra")
    PRINT "stats="; stats$

    PRINT "Completato."
END SUB
