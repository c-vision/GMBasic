' FILE: samples/Community/Ai/AiAgentAskFile.bas
' =================================================================================================
'       AI.AGENTASKFILE — l'agente riceve un input multimodale (immagine/audio/documento)
'
'       Il file è instradato a vision (immagine), STT (audio) o estrazione documento (pdf/
'       testo); il testo estratto è passato all'agente. Guida: Guide/utente/Agents-Manual.md
' =================================================================================================

IMPORT AI

SUB Main()
    ' File mancante → errore esplicito (deterministico, nessun provider richiesto).
    PRINT "ASKFILE: ", AI.AGENTASKFILE("assistant", "fattura-inesistente.pdf")

    ' Con un agente configurato e un file reale:
    '   PRINT AI.AGENTASKFILE("assistant", "fattura.pdf", "Estrai totale e IVA")
    '   PRINT AI.AGENTASKFILE("assistant", "call.wav", "Riassumi la telefonata")
    '   PRINT AI.AGENTASKFILE("assistant", "foto.jpg", "Descrivi gli oggetti")
END SUB
