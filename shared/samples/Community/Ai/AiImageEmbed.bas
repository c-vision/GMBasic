' FILE: samples/Community/Ai/AiImageEmbed.bas
' =================================================================================================
'       AI.IMAGEEMBED — embedding percettivo di un'immagine (64 double unitari)
'
'       Modello-free e deterministico: luminanza media 8x8 normalizzata. Utile per
'       near-duplicate detection e VectorDB multimodale. Guida: Guide/utente/AI-Immagini-Guide.md
' =================================================================================================

IMPORT AI
IMPORT SYSTEM

SUB Main()
    DIM src AS STRING
    src = "/Users/gaetano/Documents/Projects/GMBasicMonoRepo/shared/samples/Fixtures/ai/img_4x4.png"

    DIM e() = AI.IMAGEEMBED(src)
    PRINT "DIM: ", SYSTEM.UBOUND(e) + 1        ' 64

    ' File mancante → lista vuota (nessun crash).
    DIM z() = AI.IMAGEEMBED("immagine-inesistente.png")
    PRINT "DIM: ", SYSTEM.UBOUND(z) + 1        ' 0
END SUB
