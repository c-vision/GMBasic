' FILE: AI/AgentOperatingCenter.bas
' =================================================================
' Agent Operating Center minimo: TRACE ON/VERBOSE, event stream
' (Created/Asked/ToolCalled), AI.AGENTLIST/STATUS.
' =================================================================
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "Agent Operating Center Test"
    PRINT "============================"
    PRINT

    ' --- 1. Attiva il trace prima di creare/usare agenti ---
    AI AGENT TRACE ON
    PRINT "Trace attivato (ON)"
    PRINT

    ' --- 2. Crea due agenti e falli lavorare ---
    AI AGENT CREATE Support
    AI AGENT SET "Support" "systemPrompt" "You are a concise assistant."
    AI AGENT CREATE Researcher

    DIM answer$ = AI.AGENTASKSYNC("Support", "What is two plus two?")
    PRINT "Support ASK -> "; answer$
    PRINT

    ' --- 3. AI.AGENTLIST — tutti gli agenti registrati ---
    PRINT "--- AI.AGENTLIST() ---"
    DIM names() = AI.AGENTLIST()
    FOR i = 0 TO UBOUND(names)
        PRINT "  "; names(i)
    NEXT i
    PRINT

    ' --- 4. AI.AGENTSTATUS — riepilogo operativo di un agente ---
    PRINT "--- AI.AGENTSTATUS(""Support"") ---"
    PRINT AI.AGENTSTATUS("Support")
    PRINT

    ' --- 5. Event stream registrato dal TRACE (Created + Asked finora) ---
    PRINT "--- AI.AGENTEVENTS() ---"
    DIM events() = AI.AGENTEVENTS()
    FOR i = 0 TO UBOUND(events)
        PRINT "  "; events(i)
    NEXT i
    PRINT

    ' --- 6. Spegni il trace: da qui in poi nessun nuovo evento registrato ---
    AI AGENT TRACE OFF
    AI AGENT DELETE "Researcher"
    PRINT "Trace disattivato; evento DELETE di Researcher non registrato."
    PRINT "Numero eventi (invariato): "; UBOUND(AI.AGENTEVENTS()) + 1

    PRINT
    PRINT "Test Completato."
END SUB
