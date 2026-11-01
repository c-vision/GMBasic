' FILE: samples/Ai/AiVideoSnapshot.bas
' =================================================================================================
'       AI.VIDEOSNAPSHOT — primo frame di un video (Y4M nativo) come PNG
'
'       Ritorna 1 (ok) / 0 (errore). Output in work/. Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM v AS STRING
    v = "../Fixtures/ai/video_4x4_3f.y4m"

    DIM ok AS LONG
    ok = AI.VIDEOSNAPSHOT(v, "work/ai_video_snapshot.png")
    PRINT "SNAPSHOT: ", ok
    PRINT "INFO:     ", AI.IMAGEINFO("work/ai_video_snapshot.png")
END SUB
