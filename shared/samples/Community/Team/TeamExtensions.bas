' ==============================================================================
' FILE: Team/TeamExtensions.bas
' RecursiveMAS estensioni §9 (9.3-9.6) — superficie linguistica.
'   AI TEAM T SET DAG d,l,n                 -> AGoT DAG (9.4)
'   AI TEAM T SET COMPLEXITY t              -> soglia espansione AGoT (9.4)
'   AI TEAM T SET REFLECTION mode           -> MAR/MIRROR (9.5)
'   AI TEAM T SET CRITIC personas, judge    -> MAR critici (9.5)
'   AI TEAM T SET PRUNE topK                -> ReTreVal (9.5)
'   AI TEAM T SET STRATEGY dag              -> strategia DAG
'   AI TEAM TRAIN THINKING T, "toy"         -> Quiet-STaR distillazione (9.3)
'   AI TEAM TRAIN ACTOR CRITIC T, "toy"     -> DPSDP (9.6)
' AI AGENT SET nome thinking ON -> Quiet-STaR per agente (9.3)
' ==============================================================================
IMPORT AI WITH ALIAS

SUB Main()
    AI AGENT CREATE ExtA
    AI AGENT CREATE ExtB
 AI AGENT SET ExtA thinking ON
 AI AGENT SET ExtA thoughttokens 12
    PRINT "thinking="; AI.AGENTGET("ExtA", "thinking")
    PRINT "tokens="; AI.AGENTGET("ExtA", "thoughttokens")

    AI TEAM CREATE T
    AI TEAM T ADD AGENT ExtA
    AI TEAM T ADD AGENT ExtB
    AI TEAM T SET STRATEGY dag
    AI TEAM T SET DAG 2, 3, 3
    AI TEAM T SET COMPLEXITY 0.7
    AI TEAM T SET REFLECTION both
    AI TEAM T SET CRITIC "spec|edge|logic", ExtA
    AI TEAM T SET PRUNE 4

    AI TEAM T RUN "Decomponi e risolvi il problema."
    PRINT "run ok"

    AI TEAM TRAIN THINKING T, "toy"
    PRINT "thinking distillato"
    AI TEAM TRAIN ACTOR CRITIC T, "toy"
    PRINT "actor-critic ok"

    PRINT "Estensioni Completate."
END SUB
