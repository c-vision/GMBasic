' MarkdownAstDemo.bas — Fase 5: Markdown AST in CONVERT.
' Dimostra PARSE/HEADINGS/LINKS/TABLES/CODEBLOCKS/TREE/QUERY/RENDERHTML/
' RENDERTEXT/VALIDATE/STRIP/FORMAT sul parser C# (MdSupport) e C (gmb_md.c),
' identici su tutti e 4 i backend (IL/PCodeVM/VM/C/C99).
'
'   CONVERT.PARSE(text)      → dict AST {type, blocks:[...]}
'   CONVERT.HEADINGS(doc, l) → lista {level, text} (l<=0 = tutte)
'   CONVERT.TREE(doc)        → dump leggibile dei blocchi
'   CONVERT.QUERY(doc, sel)  → selettore "heading[level=2]"
'   CONVERT.RENDERHTML(doc)  → html | CONVERT.RENDERTEXT(doc) → testo puro
'   CONVERT.VALIDATE(text)   → "" se ok, altrimenti l'errore
'   CONVERT.STRIP(text)      → markdown → testo puro | CONVERT.FORMAT(doc) → md canonico
'
IMPORT CONVERT

SUB Main
    NL = CHR(10)
    src$ = "# Titolo" + NL + NL + _
           "Paragrafo con **bold** e [link](https://example.com)." + NL + NL + _
           "## Sotto" + NL + NL + _
           "- item1" + NL + "- item2" + NL + NL + _
           "```basic" + NL + "PRINT 1" + NL + "```"

    PRINT "=== CONVERT.PARSE + HEADINGS ==="
    d = CONVERT.PARSE(src$)
    h0 = CONVERT.HEADINGS(d, 0)
    PRINT "  heading totali: "; UBOUND(h0) + 1
    h1 = CONVERT.HEADINGS(d, 1)
    PRINT "  heading livello 1: "; UBOUND(h1) + 1
    FOR EACH h IN h1
        PRINT "    #"; STR(h.level); " "; h.text
    NEXT

    PRINT "=== LINKS / CODEBLOCKS ==="
    PRINT "  link trovati: "; UBOUND(CONVERT.LINKS(d)) + 1
    PRINT "  codeblock: "; UBOUND(CONVERT.CODEBLOCKS(d)) + 1

    PRINT "=== TREE (dump) ==="
    PRINT CONVERT.TREE(d)

    PRINT "=== QUERY ==="
    q = CONVERT.QUERY(d, "heading[level=2]")
    PRINT "  heading[level=2]: "; UBOUND(q) + 1

    PRINT "=== RENDERHTML / RENDERTEXT ==="
    PRINT "  html contiene <h1>: "; INSTR(CONVERT.RENDERHTML(d), "<h1>") > 0
    PRINT "  text contiene 'Paragrafo': "; INSTR(CONVERT.RENDERTEXT(d), "Paragrafo") > 0

    PRINT "=== VALIDATE / STRIP / FORMAT ==="
    PRINT "  validate(ok) vuoto: "; LEN(CONVERT.VALIDATE(src$)) = 0
    PRINT "  validate(fence aperta) errore: "; LEN(CONVERT.VALIDATE("ciao ``` aperto")) > 0
    PRINT "  strip rimuove il '#': "; INSTR(CONVERT.STRIP("# Titolo" + NL + "testo"), "#") = 0
    PRINT "  format canonicalizza: "; INSTR(CONVERT.FORMAT(d), "# Titolo") > 0
END SUB
