' FILE: Samples/AI/AiVision.bas
' =================================================================================================
'       AI.OCR — OCR e descrizione immagini via LLM
' =================================================================================================

IMPORT AI WITH ALIAS

SUB Main()

    ' ── OCR base ──────────────────────────────────────────────────────────

    DIM desc AS STRING

    ' Legacy: descrizione in variabile
    desc = AI.OCR("foto.jpg")
    PRINT "Descrizione: ", LEFT(desc, 300)

    desc = AI.OCR("screenshot.png")
    PRINT LEFT(desc, 300)

    ' ── Con modello specifico ─────────────────────────────────────────────

    DIM gptDesc AS STRING

    gptDesc = AI.OCR("diagramma.jpg", "gpt-4o")
    PRINT "GPT-4o: ", LEFT(gptDesc, 300)

    gptDesc = AI.OCR("diagramma.jpg", "gpt-4o")

    ' ── Batch OCR su directory ────────────────────────────────────────────

    DIM files AS LIST
    files = IO.DIR("scansioni/")

    FOR EACH f IN files
        DIM imgDesc AS STRING
        imgDesc = AI.OCR(f)
        PRINT f, ": ", LEFT(imgDesc, 100)
    NEXT

    ' ── OCR di PDF (via AI Vision) ────────────────────────────────────────

    ' Il PDF viene convertito pagina per pagina in immagini e processato
    ' (richiede provider con capacità Vision)

    gptDesc = AI.OCR("contratto.pdf", "gpt-4o")
    PRINT "PDF OCR: ", LEFT(gptDesc, 200)

END SUB
