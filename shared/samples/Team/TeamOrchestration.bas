' ==============================================================================
' FILE: Team/TeamOrchestration.bas
' TEAM RUN — orchestrazione multi-agente (strategia sequential)
' ==============================================================================
' Il team risolve un task con gli agenti membri: ogni agente riceve il contesto
' dell'agente precedente (pattern "catena di specialisti").
' ==============================================================================
IMPORT AI WITH ALIAS
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "=== TEAM Orchestration ==="
    PRINT

    ' Crea 3 agenti con ruoli
    AI AGENT CREATE Analista
    AI AGENT SET "Analista" "systemPrompt" "Sei un analista dati."
    AI AGENT CREATE Scrittore
    AI AGENT SET "Scrittore" "systemPrompt" "Sei uno scrittore tecnico."
    AI AGENT CREATE Revisore
    AI AGENT SET "Revisore" "systemPrompt" "Sei un revisore: correggi e migliora."
    PRINT "Agenti creati: Analista, Scrittore, Revisore"

    ' Crea il team e aggiungi i membri (TEAM vive dentro AI)
    AI TEAM CREATE Progetto
    AI TEAM Progetto ADD AGENT Analista
    AI TEAM Progetto ADD AGENT Scrittore
    AI TEAM Progetto ADD AGENT Revisore
    PRINT "Team 'Progetto' con 3 membri"

    ' Configura ruoli e strategia
    AI TEAM Progetto SET ROLE Analista WORKER
    AI TEAM Progetto SET ROLE Scrittore WORKER
    AI TEAM Progetto SET ROLE Revisore CRITIC
    PRINT "Ruoli configurati"

    ' Esegui l'orchestrazione (sequential: ogni agente vede l'output del precedente)
    AI TEAM Progetto RUN "Prepara un riassunto sul progetto GMBasic"
    PRINT "Orchestrazione completata"

    ' Leggi i membri del team (funzione in AI, TEAM non è namespace)
    DIM AG AS STRING
    AG = AI.TEAMAGENTS("Progetto")
    PRINT "Team finale: " + AG

    PRINT
    PRINT "Completato."
END SUB
