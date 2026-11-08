' MiniTest: COMPRESSION.DEFLATE/UNDEFLATE — round-trip (parita' 4 backend).
' EXPECT: hello world hello world hello world
REQUIRE "compression"
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello world hello world hello world"
  z$ = C.DEFLATE(s$)
  PRINT C.UNDEFLATE(z$)
END SUB
