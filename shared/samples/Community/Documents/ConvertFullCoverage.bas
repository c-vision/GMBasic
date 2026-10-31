' FILE: samples/Community/Documents/ConvertFullCoverage.bas
' Sample COMPLETO del namespace CONVERT — tutte le funzioni, deterministico.
' Markdown (Parse/Headings/Links/MdImages/Tables/CodeBlocks/Query/Tree/RenderHtml/
' RenderText/Format/Validate/Strip/Formats) + documento PDF (Open/PageCount/Text/
' Markdown/Html/Metadata/Search/Render/Extract/ExtractEngine/Close) + conversioni
' (ToHtml/ToMd/ToText) + Batch. Web richiede rete → non in questo sample.
' Output in work/ accanto al sample.
IMPORT IO
IMPORT CONVERT

SUB Main()
    PRINT "=== Markdown ==="
    DIM md$ = "# Title" + CHR(10) + "## Sub" + CHR(10) + "Hello [link](https://example.com)" + CHR(10) _
        + "![alt](img.png)" + CHR(10) _
        + "| a | b |" + CHR(10) + "|---|---|" + CHR(10) + "| 1 | 2 |" + CHR(10) _
        + "```basic" + CHR(10) + "PRINT 1" + CHR(10) + "```" + CHR(10)
    DIM d = CONVERT.PARSE(md$)
    PRINT "headings="; UBOUND(CONVERT.HEADINGS(d, 0)) + 1
    PRINT "links="; UBOUND(CONVERT.LINKS(d)) + 1
    PRINT "mdimages="; UBOUND(CONVERT.MDIMAGES(d)) + 1
    PRINT "tables="; UBOUND(CONVERT.TABLES(d)) + 1
    PRINT "codeblocks="; UBOUND(CONVERT.CODEBLOCKS(d)) + 1
    PRINT "query="; UBOUND(CONVERT.QUERY(d, "heading[level=2]")) + 1
    PRINT "tree="; CONVERT.TREE(d)
    PRINT "renderhtml="; CONVERT.RENDERHTML(d)
    PRINT "rendertext="; CONVERT.RENDERTEXT(d)
    PRINT "format="; CONVERT.FORMAT(d)
    PRINT "validate="; CONVERT.VALIDATE(md$)
    PRINT "strip="; CONVERT.STRIP(md$)
    PRINT "formats="; CONVERT.FORMATS()

    PRINT "=== Documento PDF ==="
    DIM h = CONVERT.OPEN("../../Fixtures/sample.pdf")
    PRINT "pagecount="; CONVERT.PAGECOUNT(h)
    PRINT "textlen="; LEN(CONVERT.TEXT(h))
    PRINT "mdlen="; LEN(CONVERT.MARKDOWN(h))
    PRINT "htmllen="; LEN(CONVERT.HTML(h))
    PRINT "metalen="; LEN(CONVERT.METADATA(h))
    PRINT "search="; LEN(CONVERT.SEARCH(h, "the"))
    PRINT "images="; CONVERT.IMAGES(h, 0, "work")
    PRINT "render="; CONVERT.RENDER(h, 0, 100, "work/sample.png")
    PRINT "close="; CONVERT.CLOSE(h)

    PRINT "=== Conversioni ==="
    PRINT "extract="; LEN(CONVERT.EXTRACT("../../Fixtures/sample.pdf"))
    PRINT "extractengine="; CONVERT.EXTRACTENGINE("../../Fixtures/sample.pdf")
    PRINT "tohtml="; LEN(CONVERT.TOHTML("../../Fixtures/sample.pdf"))
    PRINT "totext="; LEN(CONVERT.TOTEXT("../../Fixtures/sample.pdf"))
    PRINT "tomd="; LEN(CONVERT.TOMD("../../Fixtures/sample.pdf"))

    PRINT "=== Batch (funzione) ==="
    ON ERROR RESUME NEXT
    MKDIR "work"
    MKDIR "work/fc_in"
    ON ERROR GOTO 0
    DIM bh AS LONG
    bh = IO.FCREATE("work/fc_in/a.csv")
    IO.FWRITE(bh, "a,b" + CHR(10) + "1,2" + CHR(10))
    IO.FCLOSE(bh)
    PRINT "batchlen="; LEN(CONVERT.BATCH("work/fc_in", "md", 0))

    PRINT "=== Batch (comando, senza namespace) ==="
    CONVBAT "work/fc_in", "md", 0 INTO cbatch$
    PRINT "convbatlen="; LEN(cbatch$)

    PRINT "Sample CONVERT completo."
END SUB
