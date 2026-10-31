' MiniTest CONVERT — funzioni markdown su stringa reale (non dummy).
' Copre PARSE/TREE/RENDERHTML/RENDERTEXT/FORMAT/VALIDATE/STRIP/FORMATS.
' (HEADINGS/LINKS/MDIMAGES/TABLES/CODEBLOCKS sono in md_*.bas con fixture.)
' EXPECT: tree=#0 heading level=1|#1 heading level=2|#2 paragraph text="Hello link"|renderhtml=<html><body>|<h1>Title</h1>|<h2>Sub</h2>|<p>Hello link</p>|</body></html>|rendertext=Title|Sub|Hello link|format=# Title|## Sub|Hello link|validate=|strip=Title|Sub|Hello link|formats=txt,csv,md,html,rtf,docx,epub,odt,xlsx,pptx,pdf,url
IMPORT CONVERT

SUB Main()
    DIM md$ = "# Title" + CHR(10) + "## Sub" + CHR(10) + "Hello [link](https://example.com)" + CHR(10)
    DIM d = CONVERT.PARSE(md$)
    PRINT "tree="; CONVERT.TREE(d)
    PRINT "renderhtml="; CONVERT.RENDERHTML(d)
    PRINT "rendertext="; CONVERT.RENDERTEXT(d)
    PRINT "format="; CONVERT.FORMAT(d)
    PRINT "validate="; CONVERT.VALIDATE(md$)
    PRINT "strip="; CONVERT.STRIP(md$)
    PRINT "formats="; CONVERT.FORMATS()
END SUB
