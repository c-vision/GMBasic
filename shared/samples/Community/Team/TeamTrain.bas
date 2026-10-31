' ==============================================================================
' FILE: Team/TeamTrain.bas
' RecursiveMAS R4 — Inner-Outer training loop (TEAM vive dentro AI).
'   AI TEAM TRAIN INNER team, agent, dataset   -> regressione coseno del link interno
'   AI TEAM TRAIN OUTER team, dataset          -> allineamento R_out(h_x) -> Emb(y)
' Stato: AI.TEAMTRAINSTATUS(team), AI.TEAMLINKINFO(team)
' ==============================================================================
IMPORT AI WITH ALIAS

SUB Main()
    AI AGENT CREATE Planner
    AI AGENT CREATE Solver
    AI TEAM CREATE T
    AI TEAM T ADD AGENT Planner
    AI TEAM T ADD AGENT Solver
    AI TEAM T SET STRATEGY sequential
    AI TEAM T SET LATENT embed
    AI TEAM T SET LINK outer, "{""hidden"":64}"

    PRINT "-- TRAIN INNER --"
    AI TEAM TRAIN INNER T, Planner, "toy"
    AI TEAM TRAIN INNER T, Solver, "toy"
    PRINT "inner done"

    PRINT "-- TRAIN OUTER --"
    AI TEAM TRAIN OUTER T, "toy"
    PRINT "outer done"

    DIM status$ AS STRING
    status$ = AI.TEAMTRAINSTATUS("T")
    PRINT "status="; status$
    DIM link$ AS STRING
    link$ = AI.TEAMLINKINFO("T")
    PRINT "link="; link$

    PRINT "Training Completato."
END SUB
