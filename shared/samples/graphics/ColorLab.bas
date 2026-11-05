REM FILE: ColorLab.bas — colore e hit-test (namespace GRAPHICS).
REM Scenario: gradiente su un canvas con COLORMIX, variazioni HSL e predicati di
REM appartenenza (poligono/cerchio/rettangolo). Output deterministico, 4 backend.
IMPORT GRAPHICS

SUB Main()
  DIM c AS LONG, x AS LONG, c0 AS LONG, c7 AS LONG

  ' ── gradiente palette 1 (blu) → 15 (bianco) su 8 pixel ───────────────────────────
  CANVAS NEW 8, 1 INTO c
  FOR x = 0 TO 7
    CANVAS SET c, x, 0, GRAPHICS.COLORMIX(1, 15, x / 7.0)
  NEXT x
  CANVAS GET c, 0, 0 INTO c0
  CANVAS GET c, 7, 0 INTO c7
  PRINT "mix0="; c0; " mix7="; c7
  CANVAS FREE c

  ' ── HSL: schiarisci, scurisci, satura ───────────────────────────────────────────
  PRINT "light="; GRAPHICS.COLORLIGHTEN(1, 0.25);
  PRINT " dark="; GRAPHICS.COLORDARKEN(15, 0.25);
  PRINT " sat="; GRAPHICS.COLORSATURATE(8, 0.25)

  ' ── hit-test: quadrato (0,0)-(10,10) come 4 vertici ─────────────────────────────
  DIM p(7)
  p(0) = 0  : p(1) = 0
  p(2) = 10 : p(3) = 0
  p(4) = 10 : p(5) = 10
  p(6) = 0  : p(7) = 10
  PRINT "poly_in="; GRAPHICS.POINTINPOLYGON(5, 5, p());
  PRINT " poly_out="; GRAPHICS.POINTINPOLYGON(15, 5, p())
  PRINT "circle_in="; GRAPHICS.POINTINCIRCLE(3, 4, 0, 0, 5);
  PRINT " circle_out="; GRAPHICS.POINTINCIRCLE(6, 0, 0, 0, 5)
  PRINT "rect_in="; GRAPHICS.POINTINRECT(5, 5, 0, 0, 10, 10);
  PRINT " rect_swap="; GRAPHICS.POINTINRECT(5, 5, 10, 10, 0, 0)
END SUB
