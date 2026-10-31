IMPORT MEDIA WITH ALIAS
SUB Main()
  h = MEDIA.IMGREAD("sample.webp")
  IF h < 1 THEN
    PRINT "webp non decodificato"
  ELSE
    PRINT "webp: " + STR(MEDIA.IMGW(h)) + "x" + STR(MEDIA.IMGH(h)) + " ch=" + STR(MEDIA.IMGCHANNELS(h))
    MEDIA.IMGFREE(h)
  END IF
END SUB
