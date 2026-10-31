' MiniTest: CONVERT.HEADINGS(doc[, level]) — AST da fixture locale fix_headings.md.
' 4 heading totali (# Title, ## Sub A, ### Deep, ## Sub B): livello 1 = 1, livello 2 = 2.
' EXPECT: 4|1|2|Title
IMPORT CONVERT
IMPORT IO

SUB Main()
  DIM L = IO.READLINES("fix_headings.md")
  DIM md$ = ""
  FOR i = 0 TO UBOUND(L)
    IF i > 0 THEN md$ = md$ + CHR(10)
    md$ = md$ + L(i)
  NEXT
  DIM d = CONVERT.PARSE(md$)
  PRINT UBOUND(CONVERT.HEADINGS(d, 0)) + 1
  PRINT UBOUND(CONVERT.HEADINGS(d, 1)) + 1
  PRINT UBOUND(CONVERT.HEADINGS(d, 2)) + 1
  DIM h1 = CONVERT.HEADINGS(d, 1)
  FOR EACH h IN h1
    PRINT h.text
  NEXT
END SUB
