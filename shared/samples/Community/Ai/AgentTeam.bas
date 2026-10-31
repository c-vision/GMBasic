' FILE: AI/AgentTeam.bas
' =================================================================
' Team di agenti: creazione, configurazione, coordinamento
' =================================================================
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "Agent Team Coordination"
    PRINT "======================"
    PRINT

    ' --- 1. Crea agenti ---
    AI AGENT CREATE AgentOne
    AI AGENT SET "AgentOne" "systemPrompt" "Assistente dati."
    PRINT "AgentOne creato."

    AI AGENT CREATE AgentTwo
    AI AGENT SET "AgentTwo" "systemPrompt" "Assistente logica."
    PRINT "AgentTwo creato."
    PRINT

    ' --- 2. Configura provider ---
    AI.SETACTIVE("completion", "fixture-completion")
    AI.SETACTIVE("embedding", "fixture-embedding")
    PRINT "Provider configurati."

    ' --- 3. Interroga agenti ---
    PRINT "AgentOne: "; AI.AGENTASKSYNC("AgentOne", "Capitale Francia?")
    PRINT "AgentTwo: "; AI.AGENTASKSYNC("AgentTwo", "2 + 2?")
    PRINT

    ' --- 4. Proprieta' ---
    AI AGENT SET "AgentOne" "model" "gpt4"
    PRINT "AgentOne model: "; AI.AGENTGET("AgentOne", "model")
    PRINT

    ' --- 5. Salva/carica ---
    AI AGENT SAVE AgentOne
    AI AGENT LOAD AgentOne
    PRINT "AgentOne saved/loaded."
    PRINT

    PRINT "Test completato."
END SUB
