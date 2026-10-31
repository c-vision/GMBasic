' MiniTest: CONVERT.TABLES(doc) — AST da fixture locale fix_tables.md.
' 1 tabella (header escluso dalle righe): 2 righe dati.
' EXPECT: 1|2
IMPORT CONVERT
IMPORT IO

SUB Main()
  DIM L = IO.READLINES("fix_tables.md")
  DIM md$ = ""
  FOR i = 0 TO UBOUND(L)
    IF i > 0 THEN md$ = md$ + CHR(10)
    md$ = md$ + L(i)
  NEXT
  DIM d = CONVERT.PARSE(md$)
  DIM ts = CONVERT.TABLES(d)
  PRINT UBOUND(ts) + 1
  FOR EACH tb IN ts
    DIM rs = tb.rows
    PRINT UBOUND(rs) + 1
  NEXT
END SUB
