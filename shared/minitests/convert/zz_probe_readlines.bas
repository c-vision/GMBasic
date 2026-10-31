IMPORT IO
SUB Main()
  DIM L = IO.READLINES("fix_headings.md")
  PRINT UBOUND(L) + 1
  PRINT L(0)
END SUB
