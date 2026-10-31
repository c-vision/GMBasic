' MiniTest: CONVERT.LINKS(doc) — AST da fixture locale fix_links.md (2 link).
' EXPECT: 2|https://example.com|https://example.com/docs
IMPORT CONVERT
IMPORT IO

SUB Main()
  DIM L = IO.READLINES("fix_links.md")
  DIM md$ = ""
  FOR i = 0 TO UBOUND(L)
    IF i > 0 THEN md$ = md$ + CHR(10)
    md$ = md$ + L(i)
  NEXT
  DIM d = CONVERT.PARSE(md$)
  DIM ls = CONVERT.LINKS(d)
  PRINT UBOUND(ls) + 1
  FOR EACH l IN ls
    PRINT l.url
  NEXT
END SUB
