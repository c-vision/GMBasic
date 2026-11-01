' FILE: samples/Ai/AiImageConvert.bas
' =================================================================================================
'       AI.IMAGECONVERT — normalizza un'immagine a PNG
'
'       Ritorna 1 (ok) / 0 (errore). Output in work/. Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM src AS STRING
    src = "../Fixtures/ai/img_4x4.png"

    DIM ok AS LONG
    ok = AI.IMAGECONVERT(src, "work/ai_info_copy.png")
    PRINT "CONVERT: ", ok
    PRINT "INFO:    ", AI.IMAGEINFO("work/ai_info_copy.png")

    ' Output non PNG → 0.
    PRINT "CONVERT: ", AI.IMAGECONVERT(src, "work/x.jpg")
END SUB
