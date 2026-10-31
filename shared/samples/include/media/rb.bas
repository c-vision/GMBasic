IMPORT MEDIA WITH ALIAS
SUB Main()
  h = MEDIA.IMGREAD("jpg1024.jpg")
  PRINT "read " + STR(MEDIA.IMGW(h)) + "x" + STR(MEDIA.IMGH(h))
  PRINT "write " + STR(MEDIA.IMGWRITEQ(h, "work/rb.jpg", 90))
  MEDIA.IMGFREE(h)
END SUB
