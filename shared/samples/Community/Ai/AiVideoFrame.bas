' FILE: samples/Community/Ai/AiVideoFrame.bas
' =================================================================================================
'       AI.VIDEOFRAME — estrae un frame di un video (Y4M nativo) come PNG
'
'       Ritorna 1 (ok) / 0 (errore). Output in work/. Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM v AS STRING
    v = "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/ai/video_4x4_3f.y4m"

    DIM ok AS LONG
    ok = AI.VIDEOFRAME(v, 1, "work/ai_video_frame.png")
    PRINT "FRAME: ", ok
    PRINT "INFO:  ", AI.IMAGEINFO("work/ai_video_frame.png")

    ' Indice fuori range → 0.
    PRINT "FRAME: ", AI.VIDEOFRAME(v, 99, "work/ai_video_frame2.png")
END SUB
