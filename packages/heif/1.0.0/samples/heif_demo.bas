' heif_demo.bas — pacchetto `heif`: encode/decode HEIC via shim FFI self-contained
' (libheif+libde265+x265 compilate dai sorgenti). Usa la fixture sample.ppm accanto.
REQUIRE "heif"
IMPORT HEIF WITH ALIAS

SUB Main()
  WD$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WD$) = 0 THEN MKDIR WD$
  IN$ = PROJECTFOLDER() + "/sample.ppm"
  ENC$ = WD$ + "/demo.heic"
  BACK$ = WD$ + "/demo_back.ppm"

  PRINT "heif available = " + STR(HEIF.AVAILABLE())
  PRINT "encode = " + STR(HEIF.ENCODEPPM(IN$, ENC$, 80))

  DIM w AS LONG
  DIM h AS LONG
  PRINT "info = " + STR(HEIF.INFO(ENC$, w, h)) + " size = " + STR(w) + "x" + STR(h)

  PRINT "decode = " + STR(HEIF.DECODEPPM(ENC$, BACK$))
  PRINT "convert = " + STR(HEIF.CONVERT(ENC$, WD$ + "/demo2.ppm", 0))
END SUB
