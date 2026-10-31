' MiniTest: COMPRESSION.LZ4/UNLZ4 — round-trip (parita' 4 backend).
' EXPECT: lz4-ok
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello lz4 hello lz4 hello lz4"
  IF C.UNLZ4(C.LZ4(s$)) = s$ THEN PRINT "lz4-ok" ELSE PRINT "lz4-ko"
END SUB
