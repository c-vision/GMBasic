' MiniTest NAMESPACE CONVERT — tutte le 31 funzioni della superficie.
' Documento PDF (fixture samples/Fixtures/sample.pdf), AST Markdown (stringhe inline),
' batch su directory locale, WEB con contratto d'errore deterministico.
' EXPECT: pagecount= 1|textlen= 5022|mdlen= 41|htmllen= 111|metalen= 20|render= 0|search= 0|images= 0|close= 0|extract= 41|extractengine=PDFOXIDE|tohtml= 112|totext= 5023|tomd= 41|tree=#0 heading level=1|#1 heading level=2|#2 paragraph text="Hello link"|renderhtml=<html><body>|<h1>Title</h1>|<h2>Sub</h2>|<p>Hello link</p>|</body></html>|rendertext=Title|Sub|Hello link|format=# Title|## Sub|Hello link|validate=|strip=Title|Sub|Hello link|formats=txt,csv,md,html,rtf,docx,epub,odt,xlsx,pptx,pdf,url|query= 1|codeblocks= 2|headings0= 4|headings1= 1|links= 2|mdimages= 0|tables= 1|batch-ok|web=?Error: web fetch failed
IMPORT CONVERT
IMPORT IO

SUB TestPdf()
  DIM h = CONVERT.OPEN("../../samples/Fixtures/sample.pdf")
  PRINT "pagecount="; CONVERT.PAGECOUNT(h)
  PRINT "textlen="; LEN(CONVERT.TEXT(h))
  PRINT "mdlen="; LEN(CONVERT.MARKDOWN(h))
  PRINT "htmllen="; LEN(CONVERT.HTML(h))
  PRINT "metalen="; LEN(CONVERT.METADATA(h))
  PRINT "render="; CONVERT.RENDER(h, 0, 100, "bin/out.png")
  PRINT "search="; LEN(CONVERT.SEARCH(h, "the"))
  PRINT "images="; CONVERT.IMAGES(h, 0, "bin")
  PRINT "close="; CONVERT.CLOSE(h)

  PRINT "extract="; LEN(CONVERT.EXTRACT("../../samples/Fixtures/sample.pdf"))
  PRINT "extractengine="; CONVERT.EXTRACTENGINE("../../samples/Fixtures/sample.pdf")
  PRINT "tohtml="; LEN(CONVERT.TOHTML("../../samples/Fixtures/sample.pdf"))
  PRINT "totext="; LEN(CONVERT.TOTEXT("../../samples/Fixtures/sample.pdf"))
  PRINT "tomd="; LEN(CONVERT.TOMD("../../samples/Fixtures/sample.pdf"))
END SUB

SUB TestMarkdown()
  DIM nl AS STRING
  nl = CHR(10)
  DIM md AS STRING
  md = "# Title" + nl + "## Sub" + nl + "Hello [link](https://example.com)" + nl
  DIM d = CONVERT.PARSE(md)
  PRINT "tree="; CONVERT.TREE(d)
  PRINT "renderhtml="; CONVERT.RENDERHTML(d)
  PRINT "rendertext="; CONVERT.RENDERTEXT(d)
  PRINT "format="; CONVERT.FORMAT(d)
  PRINT "validate="; CONVERT.VALIDATE(md)
  PRINT "strip="; CONVERT.STRIP(md)
  PRINT "formats="; CONVERT.FORMATS()
  PRINT "query="; UBOUND(CONVERT.QUERY(d, "heading[level=2]")) + 1

  DIM cbmd AS STRING
  cbmd = "```basic" + nl + "PRINT 1" + nl + "```" + nl + nl + "```python" + nl + "print(2)" + nl + "```" + nl
  DIM dc = CONVERT.PARSE(cbmd)
  DIM cb = CONVERT.CODEBLOCKS(dc)
  PRINT "codeblocks="; UBOUND(cb) + 1

  DIM hmd AS STRING
  hmd = "# Title" + nl + nl + "## Sub A" + nl + nl + "### Deep" + nl + nl + "## Sub B" + nl
  DIM dh = CONVERT.PARSE(hmd)
  PRINT "headings0="; UBOUND(CONVERT.HEADINGS(dh, 0)) + 1
  PRINT "headings1="; UBOUND(CONVERT.HEADINGS(dh, 1)) + 1

  DIM lmd AS STRING
  lmd = "See [GMBasic](https://example.com) and [Docs](https://example.com/docs)." + nl
  DIM dl = CONVERT.PARSE(lmd)
  DIM ls = CONVERT.LINKS(dl)
  PRINT "links="; UBOUND(ls) + 1

  DIM imd AS STRING
  imd = "![alt one](img/a.png)" + nl + nl + "![alt two](img/b.jpg)" + nl
  DIM di = CONVERT.PARSE(imd)
  PRINT "mdimages="; UBOUND(CONVERT.MDIMAGES(di)) + 1

  DIM tmd AS STRING
  tmd = "| Name | Age |" + nl + "| ---- | --- |" + nl + "| Ada | 36 |" + nl + "| Bob | 42 |" + nl
  DIM dt = CONVERT.PARSE(tmd)
  DIM ts = CONVERT.TABLES(dt)
  PRINT "tables="; UBOUND(ts) + 1
END SUB

SUB TestBatchWeb()
  MKDIR "bin"
  MKDIR "bin/batch_in"
  DIM h AS LONG
  h = IO.FCREATE("bin/batch_in/a.csv")
  IO.FWRITE(h, "a,b" + CHR(10) + "1,2" + CHR(10))
  IO.FCLOSE(h)
  DIM r AS STRING
  r = CONVERT.BATCH("bin/batch_in", "md", 0)
  IF INSTR(r, "a.csv") > 0 AND INSTR(r, "| a b |") > 0 THEN PRINT "batch-ok" ELSE PRINT "batch-ko"
  PRINT "web="; CONVERT.WEB("http://127.0.0.1:1/x")
END SUB

SUB Main()
  MKDIR "bin"
  TestPdf()
  TestMarkdown()
  TestBatchWeb()
END SUB
