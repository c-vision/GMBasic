' StaticSiteFromMarkdown.bas — Fase 6: wiki/static-site (Markdown -> HTML).
' Legge i .md in work/src, li converte in HTML via l'AST (PARSE -> RENDERHTML)
' e genera work/site/*.html + un indice work/site/index.html con i titoli
' (estratto da HEADINGS). Output in work/ accanto al sample (scratch, gitignorata).
'
'   CONVERT.PARSE(text) → dict | CONVERT.HEADINGS(doc, 1) → titoli livello 1
'   CONVERT.RENDERHTML(doc) → html | CONVERT.RENDERTEXT(doc) → testo
'
IMPORT CONVERT
IMPORT IO WITH ALIAS

FUNCTION ReadFile(path$) AS STRING
    OPEN path$ FOR INPUT AS #1
    ReadFile = IO.FINPUT(IO.LOF(1), 1)
    CLOSE #1
END FUNCTION

SUB WritePage(title$, body$, outFile$)
    OPEN outFile FOR OUTPUT AS #1
    PRINT #1, "<!DOCTYPE html>"
    PRINT #1, "<html><head><meta charset=""utf-8""><title>" + title$ + "</title></head>"
    PRINT #1, "<body>"
    PRINT #1, "<h1>" + title$ + "</h1>"
    PRINT #1, body$
    PRINT #1, "</body></html>"
    CLOSE #1
END SUB

SUB Main
DIM g_work AS STRING
g_work = PROJECTFOLDER() + "/work"
IF NOT IO.DIREXISTS(g_work) THEN MKDIR g_work
    SRC_DIR = g_work + "/src"
    SITE_DIR = g_work + "/site"

    MKDIR SRC_DIR
    MKDIR SITE_DIR

    ' Crea i sorgenti markdown di esempio (idempotente)
    OPEN SRC_DIR + "/pagina1.md" FOR OUTPUT AS #1
    PRINT #1, "# Prima Pagina"
    PRINT #1, ""
    PRINT #1, "Testo della prima pagina con un [link](https://example.com)."
    CLOSE #1
    OPEN SRC_DIR + "/pagina2.md" FOR OUTPUT AS #1
    PRINT #1, "# Seconda Pagina"
    PRINT #1, ""
    PRINT #1, "- voce uno"
    PRINT #1, "- voce due"
    CLOSE #1

    PRINT "=== Generazione sito statico da Markdown ==="
    DIM files() AS STRING
    files = DIR(SRC_DIR + "/*.md")
    PRINT "  trovati "; UBOUND(files) + 1; " sorgenti"

    ' indice del sito (pagina -> titolo)
    OPEN SITE_DIR + "/index.html" FOR OUTPUT AS #1
    PRINT #1, "<!DOCTYPE html>"
    PRINT #1, "<html><head><meta charset=""utf-8""><title>Indice</title></head><body>"
    PRINT #1, "<h1>Indice</h1><ul>"

    FOR EACH f IN files
        src = SRC_DIR + "/" + f
        text$ = CONVERT.TOMD(src)
        d = CONVERT.PARSE(text$)
        title$ = "(senza titolo)"
        hsT = CONVERT.HEADINGS(d, 1)
        FOR EACH h IN hsT
            title$ = h.text
            EXIT FOR
        NEXT
        html$ = CONVERT.RENDERHTML(d)

        outName = SRC_DIR + "/" + f
        ' nome pagina = basename senza .md
        base$ = MID(f, 1, LEN(f) - 3)
        outPage = SITE_DIR + "/" + base$ + ".html"
        WritePage(title$, html$, outPage)

        PRINT #1, "<li><a href=""" + base$ + ".html"">" + title$ + "</a></li>"
        PRINT "  generata: " + base$ + ".html  (" + title$ + ")"
    NEXT

    PRINT #1, "</ul></body></html>"
    CLOSE #1

    PRINT "=== Sito generato in " + SITE_DIR + " ==="
END SUB
