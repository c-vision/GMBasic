' FILE: samples/Ai/AiImageDetect.bas
' =================================================================================================
'       AI.IMAGEDETECT — rilevamento oggetti (JSON con bbox) via provider vision
'
'       Richiede un provider vision attivo; file mancante → errore esplicito (nessun
'       crash). Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    ' File mancante → errore esplicito e deterministico.
    PRINT "DETECT: ", AI.IMAGEDETECT("immagine-inesistente.png")

    ' Con un file reale e un provider vision attivo:
    '   PRINT AI.IMAGEDETECT("foto.jpg")
END SUB
