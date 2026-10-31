' MediaCompressDemo.bas — MEDIA.IMGCOMPRESS: compressione adattiva (Luban-style).
IMPORT MEDIA WITH ALIAS
SUB Main()
  PRINT "auto -> " + STR(MEDIA.IMGCOMPRESS("big.jpg", "work/c_auto.jpg", 1440, 0))
  h = MEDIA.IMGREAD("work/c_auto.jpg")
  PRINT "  dims " + STR(MEDIA.IMGW(h)) + "x" + STR(MEDIA.IMGH(h))
  MEDIA.IMGFREE(h)
  PRINT "q70  -> " + STR(MEDIA.IMGCOMPRESS("big.jpg", "work/c_q70.jpg", 1440, 70))
END SUB
