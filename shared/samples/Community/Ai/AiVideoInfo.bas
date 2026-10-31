' FILE: samples/Community/Ai/AiVideoInfo.bas
' =================================================================================================
'       AI.VIDEOINFO — metadati di un video (Y4M nativo)
'
'       Ritorna "width,height,fps,frames". Formati non-Y4M richiedono ffmpeg (estensione
'       futura). Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI

SUB Main()
    DIM v AS STRING
    v = "../../../samples/Fixtures/ai/video_4x4_3f.y4m"

    PRINT "INFO: ", AI.VIDEOINFO(v)

    ' File mancante → errore esplicito.
    PRINT "INFO: ", AI.VIDEOINFO("video-inesistente.y4m")
END SUB
