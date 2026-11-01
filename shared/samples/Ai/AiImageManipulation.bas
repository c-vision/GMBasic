' FILE: samples/Ai/AiImageManipulation.bas
' =================================================================================================
'       AI.IMAGEINFO / IMAGERESIZE / IMAGECROP / IMAGECONVERT — manipolazione immagini PNG
'
'       Primitive offline e deterministiche (solo PNG, nessun modello): info dimensioni,
'       ridimensionamento, ritaglio e normalizzazione. Stessa semantica sui 4 backend
'       (IL, PCodeVM, VM/C, C99Native). Gli output vanno in work/ (locale al
'       sample). Vedi guida: Guide/utente/AI-Immagini-Guide.md (vault).
' =================================================================================================

IMPORT AI

SUB Main()
    DIM src AS STRING
    src = "../Fixtures/ai/img_4x4.png"

    ' ── INFO: "larghezza,altezza,formato" ─────────────────────────────────────
    PRINT "INFO: ", AI.IMAGEINFO(src)

    ' ── RESIZE a 8x6 ──────────────────────────────────────────────────────────
    DIM r AS LONG
    r = AI.IMAGERESIZE(src, "work/ai_image_small.png", 8, 6)
    PRINT "RESIZE: ", r
    PRINT "INFO: ", AI.IMAGEINFO("work/ai_image_small.png")

    ' ── CROP 2x2 da (1,1) ─────────────────────────────────────────────────────
    r = AI.IMAGECROP(src, "work/ai_image_crop.png", 1, 1, 2, 2)
    PRINT "CROP: ", r
    PRINT "INFO: ", AI.IMAGEINFO("work/ai_image_crop.png")

    ' ── CONVERT (normalizza a PNG) ────────────────────────────────────────────
    r = AI.IMAGECONVERT(src, "work/ai_image_copy.png")
    PRINT "CONVERT: ", r
    PRINT "INFO: ", AI.IMAGEINFO("work/ai_image_copy.png")

    ' ── Errori espliciti (mai eccezioni): file mancante ───────────────────────
    PRINT "INFO: ", AI.IMAGEINFO("inesistente.png")
END SUB
