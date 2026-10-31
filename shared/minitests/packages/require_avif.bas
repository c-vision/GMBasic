' TAG: unsupported-c99native
' EXPECT: available= 1|encode= 0|info= 0|size= 4x 3
' REQUIRE (package manager) + FFI AVIF (namespace AVIF; shim self-contained: libavif compilata
'   dai sorgenti + libaom/libvmaf statiche). Richiede il pacchetto `avif`. Deterministico.
REQUIRE "avif"
IMPORT AVIF WITH ALIAS

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$

PRINT "available="; AVIF.AVAILABLE()

DIM w AS LONG
DIM h AS LONG
PRINT "encode="; AVIF.ENCODEPPM(PROJECTFOLDER() + "/avif_sample.ppm", WK$ + "/t.avif", 80)
PRINT "info="; AVIF.INFO(WK$ + "/t.avif", w, h)
PRINT "size="; STR(w) + "x" + STR(h)
