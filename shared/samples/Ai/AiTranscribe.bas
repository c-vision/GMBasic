' FILE: Samples/AI/AiTranscribe.bas
' =================================================================================================
'       AI.TRANSCRIBE — Trascrizione audio in testo
' =================================================================================================

IMPORT AI WITH ALIAS

SUB Main()

    ' ── Trascrizione base ─────────────────────────────────────────────────

    DIM text AS STRING

    ' Trascrizione audio in variabile (funzione qualificata)
    text = AI.TRANSCRIBE("riunione.mp3")
    PRINT "Trascrizione: ", LEFT(text, 300)

    text = AI.TRANSCRIBE("lezione.m4a")
    PRINT LEFT(text, 300)

    ' ── Batch trascrizione ────────────────────────────────────────────────

    DIM files AS LIST
    files = IO.DIR("audio/")

    FOR EACH f IN files
        transcript = AI.TRANSCRIBE(f)

        DIM outName AS STRING
        outName = f + ".txt"
        PRINT f, "->", outName
    NEXT

END SUB
