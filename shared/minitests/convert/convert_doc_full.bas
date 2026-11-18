' MiniTest CONVERT — funzioni documento su PDF reale (fixture samples/Fixtures/sample.pdf).
' Copre OPEN/PAGECOUNT/TEXT/MARKDOWN/HTML/METADATA/CLOSE.
' EXPECT: pagecount= 1|textlen= 5022|mdlen= 41|htmllen= 111|metalen= 20|close= 0
IMPORT CONVERT

SUB Main()
    DIM h = CONVERT.OPEN("../../samples/Fixtures/sample.pdf")
    PRINT "pagecount="; CONVERT.PAGECOUNT(h)
    PRINT "textlen="; LEN(CONVERT.TEXT(h))
    PRINT "mdlen="; LEN(CONVERT.MARKDOWN(h))
    PRINT "htmllen="; LEN(CONVERT.HTML(h))
    PRINT "metalen="; LEN(CONVERT.METADATA(h))
    PRINT "close="; CONVERT.CLOSE(h)
END SUB
