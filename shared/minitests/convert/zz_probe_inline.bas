IMPORT CONVERT
SUB Main()
  DIM md$ = "# Title" + CHR(10) + CHR(10) + "## Sub" + CHR(10)
  DIM d = CONVERT.PARSE(md$)
  PRINT "h1arg="; UBOUND(CONVERT.HEADINGS(d)) + 1
  PRINT "valid="; LEN(CONVERT.VALIDATE(md$))
  PRINT "strip="; CONVERT.STRIP(md$)
END SUB
