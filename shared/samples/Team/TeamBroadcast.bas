' ==============================================================================
' FILE: Team/TeamBroadcast.bas
' TEAM — BROADCAST, REDEFINE e gestione membri (estensione non coperta prima)
' ==============================================================================
' Sintassi (TEAM vive dentro AI, non è namespace):
'   AI AGENT CREATE agente
'   AI TEAM CREATE nome
'   AI TEAM nome ADD AGENT agente
'   AI TEAM nome BROADCAST "testo"        ' invia il messaggio a tutti i membri
'   AI TEAM nome REDEFINE "a1,a2,a3"      ' imposta di nuovo i membri (per nome)
'   AI AGENT DELETE agente                ' rimuove un agente membro
'
' NOTA: non esiste "AI TEAM nome DELETE": per svuotare/eliminare si rimuovono
' gli agenti membri con AI AGENT DELETE (vedi commento in test).
' ==============================================================================
IMPORT AI WITH ALIAS

SUB Main()
    PRINT "=== TEAM Broadcast / Redefine ==="
    PRINT

    ' Crea gli agenti membri
    AI AGENT CREATE A1
    AI AGENT CREATE A2
    PRINT "Agenti creati: A1, A2"

    ' Crea il team e aggiunge i membri
    AI TEAM CREATE DreamTeam
    AI TEAM DreamTeam ADD AGENT A1
    AI TEAM DreamTeam ADD AGENT A2
    PRINT "Team 'DreamTeam' creato con A1, A2"

    ' Broadcast: un messaggio a tutti i membri
    AI TEAM DreamTeam BROADCAST "Hello team"
    PRINT "Broadcast inviato"

    ' Redefine: imposta di nuovo la lista membri (qui anche un nome inesistente)
    AI TEAM DreamTeam REDEFINE "A1,A2,A3"
    PRINT "Team ridefinito con A1,A2,A3"

    ' Pulizia: si eliminano gli agenti membri (non esiste TEAM DELETE)
    AI AGENT DELETE A1
    AI AGENT DELETE A2
    PRINT "Agenti eliminati"
END SUB
