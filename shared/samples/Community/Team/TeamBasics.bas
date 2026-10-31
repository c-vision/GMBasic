' ==============================================================================
' FILE: Team/TeamBasics.bas
' TEAM — creazione, membri, messaggi (prima era no-op totale)
' ==============================================================================
' Sintassi (TEAM vive dentro AI, non è namespace):
'   AI TEAM CREATE nome
'   AI TEAM nome ADD AGENT agente
'   AI TEAM nome REMOVE AGENT agente
'   AI TEAM MESSAGE nome, testo
' ==============================================================================
IMPORT AI WITH ALIAS
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "=== TEAM Basics ==="
    PRINT

    ' Crea gli agenti membri
    AI AGENT CREATE AgenteUno
    AI AGENT CREATE AgenteDue
    PRINT "Agenti creati: AgenteUno, AgenteDue"

    ' Crea il team
    AI TEAM CREATE Squadra
    PRINT "Team 'Squadra' creato"

    ' Aggiungi membri
    AI TEAM Squadra ADD AGENT AgenteUno
    AI TEAM Squadra ADD AGENT AgenteDue
    PRINT "Membri aggiunti: AgenteUno, AgenteDue"

    ' Messaggio nella conversazione
    AI TEAM MESSAGE Squadra, "Iniziamo il progetto"
    PRINT "Messaggio inviato al team"

    ' Rimuovi un membro
    AI TEAM Squadra REMOVE AGENT AgenteDue
    PRINT "Membro rimosso: AgenteDue"

    PRINT
    PRINT "Completato."
END SUB
