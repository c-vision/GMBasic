' EXPECT: rgb2hsv= 0 : 209.882352941176 , 1 , 1|hsv2rgb= 0 : 0 , 128 , 255|mix= 0 : 128 , 128 , 128|light= 0 : 72 , 72 , 255|dark= 0 : 136 , 136 , 136|cmap= 0 : 128 , 255 , 128|poly_in= 1  poly_out= 0|circ= 1  rect= 1  tri= 1|area= 100  cent= 0 : 5 , 5|closest= 0 : 5 , 0
' FFI gmbgraphics: GFXCOLOR (conversioni/mix/HSL) e GFXSHAPE (hit-test/area/centroide).
' Solo calcolo: nessun display, parità 4 backend.
IMPORT GFXCOLOR
IMPORT GFXSHAPE

SUB Main()
  DIM o(5) AS DOUBLE

  PRINT "rgb2hsv="; GFXCOLOR.RGB2HSV(0, 128, 255, o); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  PRINT "hsv2rgb="; GFXCOLOR.HSV2RGB(210, 1, 1, o); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  PRINT "mix="; GFXCOLOR.MIX(0, 0, 0, 255, 255, 255, 0.5, o); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  PRINT "light="; GFXCOLOR.LIGHTEN(0, 0, 200, 0.25, o); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  PRINT "dark="; GFXCOLOR.DARKEN(200, 200, 200, 0.25, o); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  PRINT "cmap="; GFXCOLOR.COLORMAP(0.5, o); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  DIM p(7)
  p(0) = 0  : p(1) = 0
  p(2) = 10 : p(3) = 0
  p(4) = 10 : p(5) = 10
  p(6) = 0  : p(7) = 10
  PRINT "poly_in="; GFXSHAPE.POINTINPOLYGON(5, 5, p(), 8);
  PRINT " poly_out="; GFXSHAPE.POINTINPOLYGON(15, 5, p(), 8)
  PRINT "circ="; GFXSHAPE.POINTINCIRCLE(3, 4, 0, 0, 5);
  PRINT " rect="; GFXSHAPE.POINTINRECT(5, 5, 0, 0, 10, 10);
  PRINT " tri="; GFXSHAPE.POINTINTRIANGLE(1, 1, 0, 0, 10, 0, 0, 10)
  PRINT "area="; GFXSHAPE.POLYGONAREA(p(), 8);
  PRINT " cent="; GFXSHAPE.POLYGONCENTROID(p(), 8, o); ":";
  PRINT o(0); ","; o(1)
  PRINT "closest="; GFXSHAPE.CLOSESTPOINTSEG(5, 5, 0, 0, 10, 0, o); ":";
  PRINT o(0); ","; o(1)
END SUB
