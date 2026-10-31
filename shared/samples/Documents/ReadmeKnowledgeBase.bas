' ReadmeKnowledgeBase.bas — Fase 6: parser README → knowledge base locale.
' Legge un README markdown, estrae via l'AST i titoli (indice del documento) e i
' link (riferimenti esterni), e scrive work/kb.md come knowledge base riorganizzata.
' Output in work/ accanto al sample (scratch, gitignorata).
'
'   CONVERT.PARSE(text) → dict | CONVERT.HEADINGS(doc, 0) → tutti i titoli
'   CONVERT.LINKS(doc) → link | CONVERT.RENDERTEXT(doc) → testo puro
'
IMPORT CONVERT
IMPORT IO WITH ALIAS

SUB Main
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    README_PATH = g_work + "/README.md"
    KB_PATH = g_work + "/kb.md"

    MKDIR g_work

    ' Crea un README di esempio (idempotente)
    OPEN README_PATH FOR OUTPUT AS #1
    PRINT #1, "# GMBASIC Knowledge Base"
    PRINT #1, ""
    PRINT #1, "## Documenti"
    PRINT #1, ""
    PRINT #1, "Il linguaggio supporta [PDFOxide](https://github.com/yfedoseev/pdf_oxide) "
    PRINT #1, "per l'estrazione PDF e un parser Markdown nativo."
    PRINT #1, ""
    PRINT #1, "## Installazione"
    PRINT #1, ""
    PRINT #1, "Copiare la libreria in Resources/libs."
    CLOSE #1

    PRINT "=== README → Knowledge Base ==="
    text$ = CONVERT.TOMD(README_PATH)
    d = CONVERT.PARSE(text$)

    hs = CONVERT.HEADINGS(d, 0)
    links = CONVERT.LINKS(d)

    PRINT "  titoli estratti: "; UBOUND(hs) + 1
    PRINT "  link estratti: "; UBOUND(links) + 1

    OPEN KB_PATH FOR OUTPUT AS #1
    PRINT #1, "# Knowledge Base (da README.md)"
    PRINT #1, ""
    PRINT #1, "## Indice"
    FOR EACH h IN hs
        PRINT #1, "- " + h.text
    NEXT
    PRINT #1, ""
    PRINT #1, "## Riferimenti"
    FOR EACH l IN links
        PRINT #1, "- " + l.text + " → " + l.url
    NEXT
    PRINT #1, ""
    PRINT #1, "## Contenuto (testo puro)"
    PRINT #1, CONVERT.RENDERTEXT(d)
    CLOSE #1

    PRINT "=== Knowledge base scritta in " + KB_PATH + " ==="
END SUB
