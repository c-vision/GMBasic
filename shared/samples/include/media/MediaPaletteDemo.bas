' MediaPaletteDemo.bas — MEDIA: PNG a palette 4-bit (transparency-safe), prima non supportato.
IMPORT MEDIA WITH ALIAS
SUB Main()
  h = MEDIA.IMGREAD("sprite.png")
  PRINT "sprite.png: " + STR(MEDIA.IMGW(h)) + "x" + STR(MEDIA.IMGH(h)) + " ch=" + STR(MEDIA.IMGCHANNELS(h))
  PRINT "pixel(8,7) = &H" + HEX(MEDIA.IMGGET(h, 8, 7))
  PRINT "formats = " + MEDIA.IMGFORMATS()
  MEDIA.IMGFREE(h)
END SUB
