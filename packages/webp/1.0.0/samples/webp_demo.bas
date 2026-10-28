' webp_demo.bas — pacchetto `webp`: encode/decode WebP via shim FFI self-contained (libwebp).
' Usa la fixture sample.ppm accanto a questo sample.
REQUIRE "webp"
IMPORT WEBP WITH ALIAS

SUB Main()
  WD$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WD$) = 0 THEN MKDIR WD$
  IN$ = PROJECTFOLDER() + "/sample.ppm"
  ENC$ = WD$ + "/demo.webp"
  BACK$ = WD$ + "/demo_back.ppm"

  PRINT "webp available = " + STR(WEBP.AVAILABLE())

  ' PPM -> WebP (lossy quality 90)
  PRINT "encode = " + STR(WEBP.ENCODEPPM(IN$, ENC$, 90))

  ' dimensioni del WebP prodotto (output via BYREF)
  DIM w AS LONG
  DIM h AS LONG
  PRINT "info = " + STR(WEBP.INFO(ENC$, w, h)) + " size = " + STR(w) + "x" + STR(h)

  ' WebP -> PPM
  PRINT "decode = " + STR(WEBP.DECODEPPM(ENC$, BACK$))

  ' conversione generica (estensione determina la direzione)
  PRINT "convert = " + STR(WEBP.CONVERT(ENC$, WD$ + "/demo2.ppm", 0))
END SUB
