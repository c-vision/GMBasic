' TAG: unsupported-c99native
' EXPECT: available= 1|info= 0|size= 640x 336|frame0= 0
' REQUIRE (package manager) + FFI Video (namespace VIDEO; shim self-contained: ffmpeg dai sorgenti).
REQUIRE "video"
IMPORT VIDEO WITH ALIAS

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
PRINT "available="; VIDEO.AVAILABLE()
DIM w AS LONG
DIM h AS LONG
DIM dur AS LONG
PRINT "info="; VIDEO.INFO(PROJECTFOLDER() + "/video_sample.m4v", w, h, dur)
PRINT "size="; STR(w) + "x" + STR(h)
PRINT "frame0="; VIDEO.FIRSTFRAMEPPM(PROJECTFOLDER() + "/video_sample.m4v", WK$ + "/f0.ppm")
