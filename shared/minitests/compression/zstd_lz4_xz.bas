' MiniTest: COMPRESSION.ZSTD/UNZSTD, LZ4/UNLZ4, XZ/UNXZ — round-trip (parita' 4 backend).
' EXPECT: zstd-ok|lz4-ok|xz-ok|zstd-rt|lz4-rt|xz-rt
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello compression hello compression hello compression"
  IF C.UNZSTD(C.ZSTD(s$)) = s$ THEN PRINT "zstd-ok" ELSE PRINT "zstd-ko"
  IF C.UNLZ4(C.LZ4(s$)) = s$ THEN PRINT "lz4-ok" ELSE PRINT "lz4-ko"
  IF C.UNXZ(C.XZ(s$)) = s$ THEN PRINT "xz-ok" ELSE PRINT "xz-ko"
  IF C.DECOMPRESS(C.COMPRESS(s$, "zstd"), "zstd") = s$ THEN PRINT "zstd-rt" ELSE PRINT "zstd-rt-ko"
  IF C.DECOMPRESS(C.COMPRESS(s$, "lz4"), "lz4") = s$ THEN PRINT "lz4-rt" ELSE PRINT "lz4-rt-ko"
  IF C.DECOMPRESS(C.COMPRESS(s$, "xz"), "xz") = s$ THEN PRINT "xz-rt" ELSE PRINT "xz-rt-ko"
END SUB
