' MiniTest: COMPRESSION.GZIP/UNGZIP — round-trip (parita' 4 backend).
' EXPECT: hello world hello world hello world
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello world hello world hello world"
  z$ = C.GZIP(s$)
  PRINT C.UNGZIP(z$)
END SUB
