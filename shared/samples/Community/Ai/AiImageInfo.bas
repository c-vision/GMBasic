' FILE: samples/Community/Ai/AiImageInfo.bas
' =================================================================================================
'       AI.IMAGEINFO — dimensioni e formato di un'immagine PNG
'
'       Ritorna "larghezza,altezza,formato" (es. "4,4,png"). Solo PNG.
'       Offline e deterministico. Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM path AS STRING
    path = "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/ai/img_4x4.png"

    PRINT "INFO: ", AI.IMAGEINFO(path)

    ' File mancante → errore esplicito (mai eccezione).
    PRINT "INFO: ", AI.IMAGEINFO("file-inesistente.png")
END SUB
