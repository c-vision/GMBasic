' FILE: samples/Community/Ai/AiAnalyzeImage.bas
' =================================================================================================
'       AI.ANALYZEIMAGE — descrizione/analisi di un'immagine via provider vision
'
'       Richiede un provider vision attivo (AI SET ACTIVE %OCR, alias). Offline
'       ritorna un errore esplicito (il programma NON si interrompe). Guida:
'       Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    ' File mancante → errore esplicito e deterministico (nessun modello richiesto).
    PRINT "ANALYZE: ", AI.ANALYZEIMAGE("immagine-inesistente.png", "Descrivi la scena")

    ' Con un file reale e un provider attivo:
    '   PRINT AI.ANALYZEIMAGE("foto.jpg", "Elenca gli oggetti")
END SUB
