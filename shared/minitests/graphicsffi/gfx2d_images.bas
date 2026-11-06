' EXPECT: gauss= 0 : 1.36726787450996 , 13.63273212549|sobel= 0 : 8 , 32|transp= 0 : 4 , 1|rot90= 0 : 12 , 3|fliph= 0 : 3 , 0|thr= 0 : 0 , 1|norm= 0 : 0 , 1|hist= 0 : 4 , 4 , 4 , 4|eq= 0 : 0 , 11.25|line= 0 : 2.5 , 2.5 , 2.5|med= 0 : 5|heat= 0 : 1073742079 , 1090453504|img3= 0 : 1.75 , 5.25|res= 0 : 0 , 15|stat= 0 , 15 , 15|scale= 0 : 0 , 30|sub= 0 :-1 , 14|grylum= 0 : 5000268 , 16777215|bright= 0 : 16714250|toimg= 0 : 76.245 , 255|conv3= 0 : 16711680 , 16777215|iget= 9 , 0|iset= 0 : 99 , 99|ifill= 0 : 3 , 3|iclone= 0 : 99 , 0|i3set= 0 : 7.5|i3fill= 0 : 2 , 2|bget= 255 , 0|bset= 0 : 8421504|bfill= 0 : 255 , 255|tri= 0 : 1.5 , 0
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
  PRINT "stat="; GFX2D.IMGMIN(a(), 4, 4); ","; GFX2D.IMGMAX(a(), 4, 4); ","; GFX2D.IMGABSMAX(a(), 4, 4)
  DIM sc(15) AS DOUBLE
  FOR i = 0 TO 15 : sc(i) = a(i) : NEXT i
  PRINT "scale="; GFX2D.IMGSCALE(sc(), 4, 4, 2); ":"; sc(0); ","; sc(15)
  DIM sb(15) AS DOUBLE, s2(15) AS DOUBLE
  FOR i = 0 TO 15 : s2(i) = 1 : NEXT i
  PRINT "sub="; GFX2D.IMGSUB(a(), s2(), 4, 4, sb()); ":"; sb(0); ","; sb(15)

  DIM px(3) AS DOUBLE, gb(3) AS DOUBLE, bi(3) AS DOUBLE, fi(3) AS DOUBLE
  px(0) = 16711680 : px(1) = 65280 : px(2) = 255 : px(3) = 16777215
  PRINT "grylum="; GFX2D.GRAYLUM(px(), 2, 2, gb()); ":"; gb(0); ","; gb(3)
  PRINT "bright="; GFX2D.BRIGHTNESS(px(), 2, 2, 10, 1, bi()); ":"; bi(0)
  PRINT "toimg="; GFX2D.TOIMAGE(px(), 2, 2, fi()); ":"; fi(0); ","; fi(3)
  DIM ik(8) AS DOUBLE
  ik(4) = 1
  PRINT "conv3="; GFX2D.CONVOLVE3(px(), 2, 2, ik(), 1, bi()); ":"; bi(0); ","; bi(3)

  ' helper puntuali: pixel, fill, clone, volumi, bitmap, triangolo
  DIM hp(15) AS DOUBLE, hq(15) AS DOUBLE
  FOR i = 0 TO 15 : hp(i) = a(i) : NEXT i
  DIM hp9 AS DOUBLE, hp10 AS DOUBLE
  hp9 = 0 : hp10 = 0
  PRINT "iget="; GFX2D.IMGGET(hp(), 4, 4, 1, 2); ","; GFX2D.IMGGET(hp(), 4, 4, 9, 9)
  PRINT "iset="; GFX2D.IMGSET(hp(), 4, 4, 1, 2, 99); ":"; hp(9); ","; GFX2D.IMGGET(hp(), 4, 4, 1, 2)
  PRINT "ifill="; GFX2D.IMGFILL(hq(), 4, 4, 3); ":"; hq(0); ","; hq(15)
  PRINT "iclone="; GFX2D.IMGCLONE(hp(), 4, 4, hq()); ":"; hq(9); ","; hq(0)

  DIM iv(7) AS DOUBLE
  PRINT "i3set="; GFX2D.IMG3SET(iv(), 2, 2, 2, 1, 1, 1, 7.5); ":"; GFX2D.IMG3GET(iv(), 2, 2, 2, 1, 1, 1)
  PRINT "i3fill="; GFX2D.IMG3FILL(iv(), 2, 2, 2, 2); ":"; iv(0); ","; iv(7)

  PRINT "bget="; GFX2D.BMPGET(px(), 2, 2, 0, 1); ","; GFX2D.BMPGET(px(), 2, 2, 9, 9)
  PRINT "bset="; GFX2D.BMPSET(px(), 2, 2, 1, 1, 8421504); ":"; px(3)
  PRINT "bfill="; GFX2D.BMPFILL(px(), 2, 2, 255); ":"; px(0); ","; px(3)

  DIM tri(15) AS DOUBLE, va(1) AS DOUBLE, vb(1) AS DOUBLE, vc(1) AS DOUBLE
  va(0) = 0 : va(1) = 0
  vb(0) = 3 : vb(1) = 0
  vc(0) = 0 : vc(1) = 3
  PRINT "tri="; GFX2D.DRAWTRIANGLE(tri(), 4, 4, va(), vb(), vc(), 1.5, 1); ":";
  PRINT tri(0); ","; tri(15)
END SUB
