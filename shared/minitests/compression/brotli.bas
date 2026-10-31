' MiniTest: COMPRESSION.BROTLI/UNBROTLI — round-trip (parita' 4 backend).
' EXPECT: hello world hello world hello world
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello world hello world hello world"
  z$ = C.BROTLI(s$)
  PRINT C.UNBROTLI(z$)
END SUB
