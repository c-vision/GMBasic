' EXPECT: mix= 1082163328  lit= 1090457873  dark= 1078198272  sat= 1084882944|in= 1  out= 0  circ= 1  rect= 1  rect_out= 0
' GRAPHICS: primitive colore (MIX/LIGHTEN/DARKEN/SATURATE) e hit-test
' (POINTINPOLYGON/POINTINCIRCLE/POINTINRECT). Colori = indice palette (VGA16);
' i risultati sono colori impacchettati (0x40000000|RGB).
IMPORT GRAPHICS

SUB Main()
  ' palette VGA16 disponibile senza aprire una finestra
  DIM c AS LONG
  CANVAS NEW 1, 1 INTO c
  CANVAS FREE c

  PRINT "mix="; GRAPHICS.COLORMIX(4, 11, 0.5);
  PRINT " lit="; GRAPHICS.COLORLIGHTEN(4, 0.2);
  PRINT " dark="; GRAPHICS.COLORDARKEN(4, 0.2);
  PRINT " sat="; GRAPHICS.COLORSATURATE(4, 0.5)

  DIM p(7)
  p(0) = 0  : p(1) = 0
  p(2) = 10 : p(3) = 0
  p(4) = 10 : p(5) = 10
  p(6) = 0  : p(7) = 10
  PRINT "in="; GRAPHICS.POINTINPOLYGON(5, 5, p());
  PRINT " out="; GRAPHICS.POINTINPOLYGON(15, 5, p());
  PRINT " circ="; GRAPHICS.POINTINCIRCLE(3, 4, 0, 0, 5);
  PRINT " rect="; GRAPHICS.POINTINRECT(5, 5, 0, 0, 10, 10);
  PRINT " rect_out="; GRAPHICS.POINTINRECT(15, 5, 0, 0, 10, 10)
END SUB
