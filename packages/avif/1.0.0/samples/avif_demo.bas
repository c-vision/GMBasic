' avif_demo.bas — pacchetto `avif`: encode/decode AVIF via shim FFI self-contained (libavif da sorgenti).
' Usa la fixture sample.ppm accanto a questo sample.
REQUIRE "avif"
IMPORT AVIF WITH ALIAS

SUB Main()
  WD$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WD$) = 0 THEN MKDIR WD$
  IN$ = PROJECTFOLDER() + "/sample.ppm"
  ENC$ = WD$ + "/demo.avif"
  BACK$ = WD$ + "/demo_back.ppm"

  PRINT "avif available = " + STR(AVIF.AVAILABLE())
  PRINT "encode = " + STR(AVIF.ENCODEPPM(IN$, ENC$, 80))

  DIM w AS LONG
  DIM h AS LONG
  PRINT "info = " + STR(AVIF.INFO(ENC$, w, h)) + " size = " + STR(w) + "x" + STR(h)

  PRINT "decode = " + STR(AVIF.DECODEPPM(ENC$, BACK$))
  PRINT "convert = " + STR(AVIF.CONVERT(ENC$, WD$ + "/demo2.ppm", 0))
END SUB
