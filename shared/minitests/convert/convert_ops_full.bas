' MiniTest CONVERT — operazioni documento (Render/Images/Search/Extract/ExtractEngine/
' ToHtml/ToText/ToMd) su fixture PDF reale. Batch (funzione) in convert_batch_fn.bas.
' EXPECT: render= 0|search= 0|images= 0|extract= 41|extractengine=PDFOXIDE|tohtml= 112|totext= 5023|tomd= 41
IMPORT CONVERT

SUB Main()
    MKDIR "work"
    DIM h = CONVERT.OPEN("../../samples/Fixtures/sample.pdf")
    PRINT "render="; CONVERT.RENDER(h, 0, 100, "work/out.png")
    PRINT "search="; LEN(CONVERT.SEARCH(h, "the"))
    PRINT "images="; CONVERT.IMAGES(h, 0, "work")
    PRINT "extract="; LEN(CONVERT.EXTRACT("../../samples/Fixtures/sample.pdf"))
    PRINT "extractengine="; CONVERT.EXTRACTENGINE("../../samples/Fixtures/sample.pdf")
    PRINT "tohtml="; LEN(CONVERT.TOHTML("../../samples/Fixtures/sample.pdf"))
    PRINT "totext="; LEN(CONVERT.TOTEXT("../../samples/Fixtures/sample.pdf"))
    PRINT "tomd="; LEN(CONVERT.TOMD("../../samples/Fixtures/sample.pdf"))
END SUB
