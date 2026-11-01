' FILE: samples/Ai/AiImageClassify.bas
' =================================================================================================
'       AI.IMAGECLASSIFY — classificazione tra etichette via provider vision
'
'       Richiede un provider vision attivo; file mancante → errore esplicito. Guida:
'       Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    PRINT "CLASSIFY: ", AI.IMAGECLASSIFY("immagine-inesistente.png", "fattura,ricevuta,documento")

    ' Con un file reale e un provider vision attivo:
    '   PRINT AI.IMAGECLASSIFY("doc.png", "fattura,ricevuta,documento")
END SUB
