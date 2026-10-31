' EXPECT: 4|15|15|7|3
' Pixel-level: CLS colore, LINE relativa (parte dal punto corrente), PSET, PALETTE USE.
' Leg C: C99Native (i backend managed headless non hanno readback POINT).
IMPORT GRAPHICS
SCREEN 12
CLS 4
DIM a, b, e, c, d
a = GRAPHICS.POINT(0, 0)
LINE (10, 10)-(50, 50), 15
LINE -(80, 50), 15
b = GRAPHICS.POINT(80, 50)
e = GRAPHICS.POINT(60, 50)   ' 15 solo se LINE -(...) parte dal punto corrente
PSET (100, 100), 7
c = GRAPHICS.POINT(100, 100)
DIM p(15)
FOR k = 0 TO 15
  p(k) = k * 1118481
NEXT k
PALETTE DEFINE "T", p
PALETTE USE "T"
PSET (120, 120), 3
d = GRAPHICS.POINT(120, 120)
SCREEN 0
PRINT a
PRINT b
PRINT e
PRINT c
PRINT d
