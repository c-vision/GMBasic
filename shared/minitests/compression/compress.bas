' MiniTest: COMPRESSION.COMPRESS/DECOMPRESS generico (parita' 4 backend).
' EXPECT: hello world hello world hello world
REQUIRE "compression"
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello world hello world hello world"
  z$ = C.COMPRESS(s$, "GZIP")
  PRINT C.DECOMPRESS(z$, "GZIP")
END SUB
