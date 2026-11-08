' MiniTest: COMPRESSION.ZSTD/UNZSTD — round-trip (parita' 4 backend).
' EXPECT: zstd-ok
REQUIRE "compression"
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello zstd hello zstd hello zstd"
  IF C.UNZSTD(C.ZSTD(s$)) = s$ THEN PRINT "zstd-ok" ELSE PRINT "zstd-ko"
END SUB
