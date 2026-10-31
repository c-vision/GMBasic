' MediaShrinkDemo.bas — shrink-on-load JPEG (DCT scaling): decode a 1/2..1/8, più veloce.
IMPORT MEDIA WITH ALIAS
SUB Main()
  FOR sh = 1 TO 8
    IF sh = 1 OR sh = 2 OR sh = 4 OR sh = 8 THEN
      h = MEDIA.IMGREADS("jpg1024.jpg", sh)
      IF h >= 1 THEN
        PRINT "shrink=" + STR(sh) + " -> " + STR(MEDIA.IMGW(h)) + "x" + STR(MEDIA.IMGH(h))
        MEDIA.IMGFREE(h)
      END IF
    END IF
  NEXT
END SUB
