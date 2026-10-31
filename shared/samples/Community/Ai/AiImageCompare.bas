' FILE: samples/Community/Ai/AiImageCompare.bas
' =================================================================================================
'       AI.IMAGECOMPARE — similarità tra due immagini (0..100)
'
'       Cosine degli embedding percettivi (modello-free, deterministico). Immagine con
'       se stessa → 100; file mancante → -1. Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM a AS STRING
    a = "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/ai/img_4x4.png"

    PRINT "COMPARE: ", AI.IMAGECOMPARE(a, a)          ' 100
    PRINT "COMPARE: ", AI.IMAGECOMPARE(a, "inesistente.png")  ' -1
END SUB
