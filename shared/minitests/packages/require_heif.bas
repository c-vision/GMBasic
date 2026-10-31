' TAG: unsupported-c99native
' EXPECT: available= 1|encode= 0|info= 0|size= 4x 3
' REQUIRE (package manager) + FFI HEIC/HEIF (namespace HEIF; shim self-contained: libheif+
'   libde265+x265 compilate dai sorgenti). Richiede il pacchetto `heif`. Deterministico.
REQUIRE "heif"
IMPORT HEIF WITH ALIAS

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
PRINT "available="; HEIF.AVAILABLE()

DIM w AS LONG
DIM h AS LONG
PRINT "encode="; HEIF.ENCODEPPM(PROJECTFOLDER() + "/heif_sample.ppm", WK$ + "/t.heic", 80)
PRINT "info="; HEIF.INFO(WK$ + "/t.heic", w, h)
PRINT "size="; STR(w) + "x" + STR(h)
