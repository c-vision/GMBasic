IMPORT MEDIA WITH ALIAS
SUB Main()
  h = MEDIA.IMGREAD("player.png")
  r = MEDIA.IMGROTATEA(h, 4500)   ' 45.00 gradi
  PRINT "rotate 45deg: " + STR(MEDIA.IMGW(h)) + "x" + STR(MEDIA.IMGH(h)) + " -> " + STR(MEDIA.IMGW(r)) + "x" + STR(MEDIA.IMGH(r))
  PRINT "write " + STR(MEDIA.IMGWRITE(r, "work/rot45.png"))
  MEDIA.IMGFREE(h) : MEDIA.IMGFREE(r)
END SUB
