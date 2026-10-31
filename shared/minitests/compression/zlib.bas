' MiniTest: COMPRESSION.ZLIB/UNZLIB — round-trip (parita' 4 backend).
' EXPECT: hello world hello world hello world
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello world hello world hello world"
  z$ = C.ZLIB(s$)
  PRINT C.UNZLIB(z$)
END SUB
