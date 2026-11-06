' GEO (FFI) — geometria piana: misure, predicati, forma, clip, overlay.
' EXPECT: area=16.0|per=16.0|cen= 0 =2.0,2.0|d=5.0|in= 1 , 0 |on= 1 , 0 |dps=3.0|dss=3.0|si= 1 |sp= 1 =2.0,2.0|bb= 0 =0.0,0.0,4.0,4.0|simp= 2 =4.0,0.0|tr= 0 =1.0,2.0|sc= 0 =8.0,12.0|ro= 0 =0.0,4.0|clip= 4 :4.0|ov0= 4 :16.0|ov1= 8 :-56.0|ov2= 6 :-20.0|hu= 4 :16.0|f=0
IMPORT GEO

SUB Main()
  ' quadrato (0,0)(4,0)(4,4)(0,4) in senso antiorario
  DIM xs(3) AS DOUBLE, ys(3) AS DOUBLE, o(3) AS DOUBLE
  xs(0)=0 : xs(1)=4 : xs(2)=4 : xs(3)=0
  ys(0)=0 : ys(1)=0 : ys(2)=4 : ys(3)=4
  PRINT "area="; FORMAT(GEO.POLYAREA(xs, ys, 4), "#.0");
  PRINT "|per="; FORMAT(GEO.PERIMETER(xs, ys, 4, 1), "#.0");
  PRINT "|cen="; GEO.CENTROID(xs, ys, 4, o); "="; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0");
  PRINT "|d="; FORMAT(GEO.DIST(0, 0, 3, 4), "#.0");
  PRINT "|in="; GEO.POINTINPOLY(2, 2, xs, ys, 4); ","; GEO.POINTINPOLY(5, 5, xs, ys, 4);
  PRINT "|on="; GEO.ONSEGMENT(2, 0, 0, 0, 4, 0); ","; GEO.ONSEGMENT(2, 1, 0, 0, 4, 0);
  PRINT "|dps="; FORMAT(GEO.DISTPOINTSEG(2, 3, 0, 0, 4, 0), "#.0");
  PRINT "|dss="; FORMAT(GEO.DISTSEGSEG(0, 0, 4, 0, 0, 3, 4, 3), "#.0");
  PRINT "|si="; GEO.SEGINTERSECT(0, 0, 4, 4, 0, 4, 4, 0);
  PRINT "|sp="; GEO.SEGINTERSECTION(o, 0, 0, 4, 4, 0, 4, 4, 0); "="; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0");
  PRINT "|bb="; GEO.BBOX(xs, ys, 4, o); "="; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); ","; FORMAT(o(2), "#.0"); ","; FORMAT(o(3), "#.0");

  ' polilinea quasi retta: Douglas-Peucker con tol 0.5 lascia i due estremi
  DIM lx(4) AS DOUBLE, ly(4) AS DOUBLE, sx(4) AS DOUBLE, sy(4) AS DOUBLE
  lx(0)=0 : lx(1)=1 : lx(2)=2 : lx(3)=3 : lx(4)=4
  ly(0)=0 : ly(1)=0.1 : ly(2)=0 : ly(3)=0 : ly(4)=0
  PRINT "|simp="; GEO.SIMPLIFY(lx, ly, 5, 0.5, sx, sy, 5); "="; FORMAT(sx(1), "#.0"); ","; FORMAT(sy(1), "#.0");
  PRINT "|tr="; GEO.TRANSLATE(xs, ys, 4, 1, 2, sx, sy); "="; FORMAT(sx(0), "#.0"); ","; FORMAT(sy(0), "#.0");
  PRINT "|sc="; GEO.SCALE(xs, ys, 4, 2, 3, sx, sy); "="; FORMAT(sx(1), "#.0"); ","; FORMAT(sy(2), "#.0");
  PRINT "|ro="; GEO.ROTATE(xs, ys, 4, 90, sx, sy); "="; FORMAT(sx(1), "#.0"); ","; FORMAT(sy(1), "#.0");

  ' clip: quadrato ∩ rettangolo (2,2)-(6,6) -> area 4
  DIM cx(3) AS DOUBLE, cy(3) AS DOUBLE, kx(7) AS DOUBLE, ky(7) AS DOUBLE
  cx(0)=2 : cx(1)=6 : cx(2)=6 : cx(3)=2
  cy(0)=2 : cy(1)=2 : cy(2)=6 : cy(3)=6
  PRINT "|clip="; GEO.CLIP(xs, ys, 4, cx, cy, 4, kx, ky, 8);
  PRINT ":"; FORMAT(GEO.POLYAREA(kx, ky, 4), "#.0");

  ' overlay: A=(0,0)-(6,6), B=(2,2)-(8,8) -> intersezione 16, unione 56, differenza 20
  DIM ax(3) AS DOUBLE, ay(3) AS DOUBLE, bx(3) AS DOUBLE, by(3) AS DOUBLE
  ax(0)=0 : ax(1)=6 : ax(2)=6 : ax(3)=0
  ay(0)=0 : ay(1)=0 : ay(2)=6 : ay(3)=6
  bx(0)=2 : bx(1)=8 : bx(2)=8 : bx(3)=2
  by(0)=2 : by(1)=2 : by(2)=8 : by(3)=8
  DIM ox(15) AS DOUBLE, oy(15) AS DOUBLE, n AS LONG
  n = GEO.OVERLAY(ax, ay, 4, bx, by, 4, ox, oy, 16, 0)
  PRINT "|ov0="; n; ":"; FORMAT(GEO.POLYAREA(ox, oy, n), "#.0");
  n = GEO.OVERLAY(ax, ay, 4, bx, by, 4, ox, oy, 16, 1)
  PRINT "|ov1="; n; ":"; FORMAT(GEO.POLYAREA(ox, oy, n), "#.0");
  n = GEO.OVERLAY(ax, ay, 4, bx, by, 4, ox, oy, 16, 2)
  PRINT "|ov2="; n; ":"; FORMAT(GEO.POLYAREA(ox, oy, n), "#.0");
  DIM hx(7) AS DOUBLE, hy(7) AS DOUBLE
  DIM hxs(4) AS DOUBLE, hys(4) AS DOUBLE
  hxs(0)=0 : hxs(1)=4 : hxs(2)=4 : hxs(3)=0 : hxs(4)=2
  hys(0)=0 : hys(1)=0 : hys(2)=4 : hys(3)=4 : hys(4)=2
  PRINT "|hu="; GEO.CONVEXHULL(hxs, hys, 5, hx, hy, 8); ":"; FORMAT(GEO.POLYAREA(hx, hy, 4), "#.0");
  PRINT "|f=0"
END SUB
