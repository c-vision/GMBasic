' TAG: unsupported-c99native
' EXPECT: available= 1|encode= 0|info= 0|size= 4x 3
' REQUIRE (package manager) + FFI WebP (namespace WEBP, shim self-contained con libwebp statica).
'   Richiede il pacchetto `webp` installato in ~/.gmb/packages. Deterministico (fixture PPM locale).
REQUIRE "webp"
IMPORT WEBP WITH ALIAS

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$

PRINT "available="; WEBP.AVAILABLE()

DIM w AS LONG
DIM h AS LONG
PRINT "encode="; WEBP.ENCODEPPM(PROJECTFOLDER() + "/webp_sample.ppm", WK$ + "/t.webp", 90)
PRINT "info="; WEBP.INFO(WK$ + "/t.webp", w, h)
PRINT "size="; STR(w) + "x" + STR(h)
