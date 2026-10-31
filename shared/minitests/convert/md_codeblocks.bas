' MiniTest: CONVERT.CODEBLOCKS(doc) — AST da fixture locale fix_codeblocks.md.
' 2 blocchi fenced con linguaggi basic e python.
' EXPECT: 2|basic|python
IMPORT CONVERT
IMPORT IO

SUB Main()
  DIM L = IO.READLINES("fix_codeblocks.md")
  DIM md$ = ""
  FOR i = 0 TO UBOUND(L)
    IF i > 0 THEN md$ = md$ + CHR(10)
    md$ = md$ + L(i)
  NEXT
  DIM d = CONVERT.PARSE(md$)
  DIM cb = CONVERT.CODEBLOCKS(d)
  PRINT UBOUND(cb) + 1
  FOR EACH b IN cb
    PRINT b.language
  NEXT
END SUB
