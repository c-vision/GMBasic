' MiniTest: COMPRESSION.BZIP2/UNBZIP2 — round-trip (parita' 4 backend).
' EXPECT: hello world hello world hello world
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello world hello world hello world"
  z$ = C.BZIP2(s$)
  PRINT C.UNBZIP2(z$)
END SUB
