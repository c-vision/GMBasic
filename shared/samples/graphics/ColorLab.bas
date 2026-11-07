REM FILE: ColorLab.bas — colore e hit-test via FFI (gfxcolor.inc / gfxshape.inc).
REM Scenario: gradiente calcolato con GFXCOLOR.MIX e disegnato su un canvas con CANVAS SET,
REM poi predicati GFXSHAPE su poligono/cerchio/rettangolo. Output deterministico, 4 backend.
IMPORT GRAPHICS
IMPORT GFXCOLOR
IMPORT GFXSHAPE

' Colore impacchettato atteso da PSET/CANVAS SET: 0x40000000 | RGB.
FUNCTION PACKRGB(r, g, b) AS LONG
  PACKRGB = 1073741824 + INT(r) * 65536 + INT(g) * 256 + INT(b)
END FUNCTION

SUB Main()
  DIM o(5) AS DOUBLE
  DIM c AS LONG, x AS LONG, col AS LONG

  ' ── gradiente blu -> bianco calcolato con la libreria ───────────────────────────
  CANVAS NEW 8, 1 INTO c
  FOR x = 0 TO 7
    ok = GFXCOLOR.MIX(0, 0, 255, 255, 255, 255, x / 7.0, o)
    col = PACKRGB(o(0), o(1), o(2))
    CANVAS SET c, x, 0, col
  NEXT x
  CANVAS GET c, 0, 0 INTO col
  PRINT "grad0="; col;
  CANVAS GET c, 7, 0 INTO col
  PRINT " grad7="; col
  CANVAS FREE c

  ' ── conversioni e HSL ──────────────────────────────────────────────────────────
  PRINT "rgb2hsv="; GFXCOLOR.RGB2HSV(0, 128, 255, o); ":";
  PRINT o(0); ","; o(1); ","; o(2)
  PRINT "light="; GFXCOLOR.LIGHTEN(0, 0, 200, 0.25, o); ":";
  PRINT o(0); ","; o(1); ","; o(2)
  PRINT "dark="; GFXCOLOR.DARKEN(200, 200, 200, 0.25, o); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  ' ── hit-test ───────────────────────────────────────────────────────────────────
  DIM p(7)
  p(0) = 0  : p(1) = 0
  p(2) = 10 : p(3) = 0
  p(4) = 10 : p(5) = 10
  p(6) = 0  : p(7) = 10
  PRINT "poly_in="; GFXSHAPE.POINTINPOLYGON(5, 5, p()) ;
  PRINT " poly_out="; GFXSHAPE.POINTINPOLYGON(15, 5, p()) 
  PRINT "circ="; GFXSHAPE.POINTINCIRCLE(3, 4, 0, 0, 5);
  PRINT " rect="; GFXSHAPE.POINTINRECT(5, 5, 0, 0, 10, 10);
  PRINT " area="; GFXSHAPE.POLYGONAREA(p()) 
END SUB
