' FILE: samples/Community/Ai/AiImageCrop.bas
' =================================================================================================
'       AI.IMAGECROP — ritaglia un PNG (x, y, larghezza, altezza)
'
'       Ritorna 1 (ok) / 0 (errore); clamp automatico ai bordi. Output in work/.
'       Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM src AS STRING
    src = "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/ai/img_4x4.png"

    DIM ok AS LONG
    ok = AI.IMAGECROP(src, "work/ai_info_crop.png", 1, 1, 2, 2)
    PRINT "CROP: ", ok
    PRINT "INFO: ", AI.IMAGEINFO("work/ai_info_crop.png")

    ' Rettangolo fuori immagine → 0.
    PRINT "CROP: ", AI.IMAGECROP(src, "work/y.png", 100, 100, 2, 2)
END SUB
