' EXPECT: gauss= 0 : 1.36726787450996 , 13.63273212549|sobel= 0 : 8 , 32|transp= 0 : 4 , 1|rot90= 0 : 12 , 3|fliph= 0 : 3 , 0|thr= 0 : 0 , 1|norm= 0 : 0 , 1|hist= 0 : 4 , 4 , 4 , 4|eq= 0 : 0 , 11.25|line= 0 : 2.5 , 2.5 , 2.5|med= 0 : 5|heat= 0 : 1073742079 , 1090453504|img3= 0 : 1.75 , 5.25|res= 0 : 0 , 15
' FFI gmbgraphics: GFX2D — immagini float (filtri, trasformazioni, istogrammi, disegno).
' Solo calcolo: nessun display, parità 4 backend.
IMPORT GFX2D

SUB Main()
  DIM a(15) AS DOUBLE, o(15) AS DOUBLE, gx(15) AS DOUBLE, gy(15) AS DOUBLE
  FOR i = 0 TO 15 : a(i) = i : NEXT i

  PRINT "gauss="; GFX2D.GAUSSIAN(a(), 4, 4, 0.8, o); ":";
  PRINT o(0); ","; o(15)
  PRINT "sobel="; GFX2D.SOBEL(a(), 4, 4, gx(), gy()); ":";
  PRINT gx(5); ","; gy(5)
  PRINT "transp="; GFX2D.TRANSPOSE(a(), 4, 4, o); ":";
  PRINT o(1); ","; o(4)
  PRINT "rot90="; GFX2D.ROTATE90(a(), 4, 4, 1, o); ":";
  PRINT o(0); ","; o(15)
  PRINT "fliph="; GFX2D.FLIPH(a(), 4, 4, o); ":";
  PRINT o(0); ","; o(3)
  PRINT "thr="; GFX2D.THRESHOLD(a(), 4, 4, 7.5, 0, 1, o); ":";
  PRINT o(0); ","; o(15)
  PRINT "norm="; GFX2D.NORMALIZE(a(), 4, 4, 0, 1, o); ":";
  PRINT o(0); ","; o(15)

  DIM hist(3) AS DOUBLE
  PRINT "hist="; GFX2D.HISTOGRAM(a(), 4, 4, 4, 0, 16, hist()); ":";
  PRINT hist(0); ","; hist(1); ","; hist(2); ","; hist(3)
  PRINT "eq="; GFX2D.HISTEQUALIZE(a(), 4, 4, 4, o); ":";
  PRINT o(0); ","; o(15)

  DIM d(15) AS DOUBLE
  PRINT "line="; GFX2D.DRAWLINE(d(), 4, 4, 0, 0, 3, 3, 2.5); ":";
  PRINT d(0); ","; d(5); ","; d(15)
  PRINT "med="; GFX2D.MEDIAN3(a(), 4, 4, o); ":";
  PRINT o(5)

  DIM z(3) AS DOUBLE, hm(3) AS DOUBLE
  z(0) = 0 : z(1) = 1 : z(2) = 2 : z(3) = 3
  PRINT "heat="; GFX2D.HEATMAP(z(), 2, 2, 0, 3, hm()); ":";
  PRINT hm(0); ","; hm(3)

  DIM vol(7) AS DOUBLE, vk(2) AS DOUBLE, vo(7) AS DOUBLE
  FOR i = 0 TO 7 : vol(i) = i : NEXT i
  vk(0) = 0.25 : vk(1) = 0.5 : vk(2) = 0.25
  PRINT "img3="; GFX2D.IMG3CONVOLVE(vol(), 2, 2, 2, vk(), 3, vo()); ":";
  PRINT vo(0); ","; vo(7)

  DIM r(3) AS DOUBLE
  PRINT "res="; GFX2D.RESAMPLE(a(), 4, 4, 2, 2, r()); ":";
  PRINT r(0); ","; r(3)
END SUB
