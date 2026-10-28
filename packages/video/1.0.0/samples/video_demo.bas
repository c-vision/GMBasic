' video_demo.bas — pacchetto `video`: probe + estrazione frame via shim FFI self-contained
' (ffmpeg compilato dai sorgenti). Usa la fixture sample.m4v accanto a questo sample.
REQUIRE "video"
IMPORT VIDEO WITH ALIAS

SUB Main()
  WD$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WD$) = 0 THEN MKDIR WD$
  IN$ = PROJECTFOLDER() + "/sample.m4v"
  PPM$ = WD$ + "/frame0.ppm"

  PRINT "video available = " + STR(VIDEO.AVAILABLE())

  DIM w AS LONG
  DIM h AS LONG
  DIM dur AS LONG
  PRINT "info = " + STR(VIDEO.INFO(IN$, w, h, dur)) + " size = " + STR(w) + "x" + STR(h) + " dur = " + STR(dur) + "ms"

  PRINT "frame0 = " + STR(VIDEO.FIRSTFRAMEPPM(IN$, PPM$))
END SUB
