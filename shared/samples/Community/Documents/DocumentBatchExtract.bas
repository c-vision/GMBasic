' DocumentBatchExtract.bas — Fase 6: pipeline documentale batch.
' Estrae il markdown da ogni PDF in una cartella sorgente e salva i .md in work/.
' Usa il namespace DOCUMENT (Fase 4): handle-based via PDFOxide (stessa lib su
' tutti i backend). Esempio: wiki/static-site, knowledge base locale.
'
'   CONVERT.OPEN(src) → handle | CONVERT.MARKDOWN(h, -1) → tutto il documento
'   CONVERT.METADATA(h) → "pages=N; version=M.m" | CONVERT.CLOSE(h)
'
IMPORT CONVERT
IMPORT IO WITH ALIAS

SUB Main
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    ' Cartella sorgente: qui i PDF (fixture in samples/Fixtures/sample.pdf)
    ' Cartella di output: work/ accanto al sample (scratch, gitignorata)
    SRC_DIR = g_work + "/src"
    OUT_DIR = g_work + "/out"

    MKDIR SRC_DIR
    MKDIR OUT_DIR

    ' Copia il PDF di esempio nella cartella sorgente (idempotente)
    COPY "../../../samples/Fixtures/sample.pdf" TO SRC_DIR + "/doc1.pdf"
    COPY "../../../samples/Fixtures/sample.pdf" TO SRC_DIR + "/doc2.pdf"

    ' Elenca i file PDF nella cartella sorgente (DIR ritorna un array di stringhe)
    DIM files() AS STRING
    files = DIR(SRC_DIR + "/*.pdf")
    PRINT "  trovati " + STR(UBOUND(files) + 1) + " PDF"

    PRINT "=== Pipeline batch PDF -> Markdown ==="
    n = 0
    FOR EACH f IN files
        src = SRC_DIR + "/" + f
        h = CONVERT.OPEN(src)
        IF h > 0 THEN
            md = CONVERT.MARKDOWN(h, -1)
            meta = CONVERT.METADATA(h)
            outFile = OUT_DIR + "/doc" + STR(n) + ".md"
            ' scrive il markdown su file (OPEN FOR OUTPUT + PRINT #)
            OPEN outFile FOR OUTPUT AS #1
            PRINT #1, md
            CLOSE #1
            PRINT "  estratto: " + f + "  [" + meta + "]  md_len=" + STR(LEN(md))
            CONVERT.CLOSE(h)
            n = n + 1
        ELSE
            PRINT "  ERRORE apertura: " + f
        END IF
    NEXT

    PRINT "=== Fatto: " + STR(n) + " documenti estratti in " + OUT_DIR + " ==="
END SUB
