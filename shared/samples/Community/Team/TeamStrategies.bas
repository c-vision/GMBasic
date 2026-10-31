' ==============================================================================
' FILE: Team/TeamStrategies.bas
' RecursiveMAS R3 — strategie di orchestrazione avanzate (TEAM vive dentro AI).
'   Mixture       : esperti in parallelo -> summarizer aggrega
'   Distillation  : Expert -> Learner (il learner raffina)
'   Deliberation  : Reflector critica -> ToolCaller agisce (loop per RECURSION)
' 'embed' (R2): modalita latent proxy con RecursiveLink (fingerprint nel contesto).
' ==============================================================================
IMPORT AI WITH ALIAS

SUB Main()
    ' Mixture: 3 esperti + 1 aggregatore
    AI AGENT CREATE MathExp
    AI AGENT CREATE CodeExp
    AI AGENT CREATE SciExp
    AI AGENT CREATE Summ
    AI TEAM CREATE Mix
    AI TEAM Mix ADD AGENT MathExp
    AI TEAM Mix ADD AGENT CodeExp
    AI TEAM Mix ADD AGENT SciExp
    AI TEAM Mix ADD AGENT Summ
    AI TEAM Mix SET STRATEGY mixture
    AI TEAM Mix RUN "Problema GPQA: organizza la risposta"
    PRINT "Mixture ok"

    ' Distillation: Expert -> Learner
    AI AGENT CREATE Expert
    AI AGENT CREATE Learner
    AI TEAM CREATE Dist
    AI TEAM Dist ADD AGENT Expert
    AI TEAM Dist ADD AGENT Learner
    AI TEAM Dist SET STRATEGY distillation
    AI TEAM Dist RUN "Semplifica la spiegazione"
    PRINT "Distillation ok"

    ' Deliberation con latent embed (2 round)
    AI AGENT CREATE Reflector
    AI AGENT CREATE Tooler
    AI TEAM CREATE Delib
    AI TEAM Delib ADD AGENT Reflector
    AI TEAM Delib ADD AGENT Tooler
    AI TEAM Delib SET STRATEGY deliberation
    AI TEAM Delib SET RECURSION 2
    AI TEAM Delib SET LATENT embed
    AI TEAM Delib SET LINK outer, "{""hidden"":64}"
    AI TEAM Delib RUN "Debug di un loop infinito"
    PRINT "Deliberation embed ok"

    PRINT "Strategie Completate."
END SUB
