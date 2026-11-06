' EXPECT: conv= 0 : 0 , 5 , 15|csep= 0 : 0 , 6 , 15|rot0= 0 : 12|rot90= 0 : 10.9999998660255 , 12|flipv= 0 : 12 , 3|rectf= 0 : 5 , 5 , 0|rect= 0 : 7 , 7 , 7 , 0|circ= 0 : 0 , 1 , 0|warp= 0 : 1 , 4|pois= 0 : 1 , 0|gray= 0 : 16777215 , 5592405 , 5592405
' FFI gmbgraphics: GFX2D — convoluzioni, rotazione, disegno rettangoli/cerchi,
' warping, Poisson blend e grayscale naive. Solo calcolo, parità 4 backend.
IMPORT GFX2D

SUB Main()
  DIM a(15) AS DOUBLE, o(15) AS DOUBLE, d(15) AS DOUBLE
  DIM k(8) AS DOUBLE, kx(2) AS DOUBLE, ky(2) AS DOUBLE
  FOR i = 0 TO 15 : a(i) = i : NEXT i
  k(4) = 1
  kx(0) = 0 : kx(1) = 1 : kx(2) = 0
  ky(0) = 0 : ky(1) = 1 : ky(2) = 0

  PRINT "conv="; GFX2D.CONVOLVE(a(), 4, 4, k(), 3, 3, o()); ":"; o(0); ","; o(5); ","; o(15)
  PRINT "csep="; GFX2D.CONVOLVESEP(a(), 4, 4, kx(), 3, ky(), 3, o()); ":"; o(0); ","; o(6); ","; o(15)
  DIM m(24) AS DOUBLE, mo(24) AS DOUBLE
  FOR i = 0 TO 24 : m(i) = i : NEXT i
  PRINT "rot0="; GFX2D.ROTATE(m(), 5, 5, 0, -1, mo()); ":"; mo(12)
  PRINT "rot90="; GFX2D.ROTATE(m(), 5, 5, 1.5707963, -1, mo()); ":"; mo(7); ","; mo(12)
  PRINT "flipv="; GFX2D.FLIPV(a(), 4, 4, o()); ":"; o(0); ","; o(15)

  PRINT "rectf="; GFX2D.DRAWRECTF(d(), 4, 4, 0, 0, 2, 2, 5); ":";
  PRINT d(0); ","; d(5); ","; d(15)
  DIM e(15) AS DOUBLE
  PRINT "rect="; GFX2D.DRAWRECT(e(), 4, 4, 1, 1, 2, 2, 7); ":";
  PRINT e(5); ","; e(6); ","; e(9); ","; e(15)
  DIM f(63) AS DOUBLE
  PRINT "circ="; GFX2D.DRAWCIRCLE(f(), 8, 8, 3, 3, 2, 1, 0); ":";
  PRINT f(3 + 8 * 3); ","; f(3 + 8 * 1); ","; f(0)

  DIM im(3) AS DOUBLE, fx(3) AS DOUBLE, fy(3) AS DOUBLE, wo(3) AS DOUBLE
  im(0) = 1 : im(1) = 2 : im(2) = 3 : im(3) = 4
  PRINT "warp="; GFX2D.WARP(im(), 1, 2, 2, fx(), fy(), wo()); ":";
  PRINT wo(0); ","; wo(3)

  DIM tg(8) AS DOUBLE, sr(8) AS DOUBLE, mk(8) AS DOUBLE, po(8) AS DOUBLE
  sr(4) = 1
  mk(4) = 1
  PRINT "pois="; GFX2D.POISSON(tg(), sr(), mk(), 3, 3, po(), 30); ":";
  PRINT po(4); ","; po(0)

  DIM px(3) AS DOUBLE, gr(3) AS DOUBLE
  px(0) = 16777215 : px(1) = 0 : px(2) = 255 : px(3) = 16711680
  PRINT "gray="; GFX2D.GRAYNAIVE(px(), 2, 2, gr()); ":";
  PRINT gr(0); ","; gr(2); ","; gr(3)
END SUB
