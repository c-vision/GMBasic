' FILE: samples/Community/Ai/AiImageResize.bas
' =================================================================================================
'       AI.IMAGERESIZE — ridimensiona un PNG (nearest-neighbor)
'
'       Ritorna 1 (ok) / 0 (errore). L'output va in work/ (locale al sample).
'       Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM src AS STRING
    src = "../../../samples/Fixtures/ai/img_4x4.png"

    DIM ok AS LONG
    ok = AI.IMAGERESIZE(src, "work/ai_info_resize.png", 16, 16)
    PRINT "RESIZE: ", ok
    PRINT "INFO:   ", AI.IMAGEINFO("work/ai_info_resize.png")

    ' Dimensioni non valide → 0.
    PRINT "RESIZE: ", AI.IMAGERESIZE(src, "work/x.png", 0, 0)
END SUB
