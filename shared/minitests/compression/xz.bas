' MiniTest: COMPRESSION.XZ/UNXZ — round-trip (parita' 4 backend).
' EXPECT: xz-ok
IMPORT COMPRESSION AS C

SUB Main
  s$ = "hello xz hello xz hello xz"
  IF C.UNXZ(C.XZ(s$)) = s$ THEN PRINT "xz-ok" ELSE PRINT "xz-ko"
END SUB
