' GEO (FFI) — geometria piana: misure, predicati, forma, clip, overlay.
' Geometrie come ARRAY di GeoPoint (TYPE a due double: layout della struct C).
' EXPECT: area=16.0|per=16.0|cen=2.0,2.0|d=5.0|in= 1 , 0 |on= 1 , 0 |dps=3.0|dss=3.0|si= 1 |sp= 1 =2.0,2.0|bb=0.0,0.0,4.0,4.0|simp= 2 =4.0,0.0|tr= 0 =1.0,2.0|sc= 0 =8.0,12.0|ro= 0 =0.0,4.0|clip= 4 :4.0|ov0= 4 :16.0|ov1= 8 :-56.0|ov2= 6 :-20.0|hu= 4 :16.0|f=0
IMPORT GEO

SUB Main()
  ' quadrato (0,0)(4,0)(4,4)(0,4) in senso antiorario
  DIM q(3) AS GeoPoint
  q(0).X=0 : q(0).Y=0
  q(1).X=4 : q(1).Y=0
  q(2).X=4 : q(2).Y=4
  q(3).X=0 : q(3).Y=4
  PRINT "area="; FORMAT(GEO.POLYAREA(q, 4), "#.0");
  PRINT "|per="; FORMAT(GEO.PERIMETER(q, 4, 1), "#.0");
  DIM c AS GeoPoint
  c = GEO.CENTROID(q, 4)
  PRINT "|cen="; FORMAT(c.X, "#.0"); ","; FORMAT(c.Y, "#.0");
  DIM a AS GeoPoint, b AS GeoPoint
  a.X=0 : a.Y=0 : b.X=3 : b.Y=4
  PRINT "|d="; FORMAT(GEO.DIST(a, b), "#.0");
  DIM p2 AS GeoPoint
  p2.X=2 : p2.Y=2
  PRINT "|in="; GEO.POINTINPOLY(p2, q, 4);
  p2.X=5 : p2.Y=5
  PRINT ","; GEO.POINTINPOLY(p2, q, 4);
  a.X=2 : a.Y=0 : b.X=0 : b.Y=0
  DIM e AS GeoPoint
  e.X=4 : e.Y=0
  PRINT "|on="; GEO.ONSEGMENT(a, b, e);
  a.Y=1
  PRINT ","; GEO.ONSEGMENT(a, b, e);
  ' punto (2,3) e segmento (0,0)-(4,0)
  a.X=2 : a.Y=3 : b.X=0 : b.Y=0
  PRINT "|dps="; FORMAT(GEO.DISTPOINTSEG(a, b, e), "#.0");
  ' segmenti (0,0)-(4,0) e (0,3)-(4,3)
  DIM s3 AS GeoPoint, s4 AS GeoPoint
  s3.X=0 : s3.Y=3 : s4.X=4 : s4.Y=3
  PRINT "|dss="; FORMAT(GEO.DISTSEGSEG(b, e, s3, s4), "#.0");
  ' intersezione (0,0)-(4,4) × (0,4)-(4,0)
  DIM d1 AS GeoPoint, d2 AS GeoPoint, d3 AS GeoPoint, d4 AS GeoPoint
  d1.X=0 : d1.Y=0 : d2.X=4 : d2.Y=4
  d3.X=0 : d3.Y=4 : d4.X=4 : d4.Y=0
  PRINT "|si="; GEO.SEGINTERSECT(d1, d2, d3, d4);
  DIM ip(0) AS GeoPoint
  PRINT "|sp="; GEO.SEGINTERSECTION(d1, d2, d3, d4, ip); "="; FORMAT(ip(0).X, "#.0"); ","; FORMAT(ip(0).Y, "#.0");
  DIM bb AS GeoBox
  bb = GEO.BBOX(q, 4)
  PRINT "|bb="; FORMAT(bb.MinX, "#.0"); ","; FORMAT(bb.MinY, "#.0"); ","; FORMAT(bb.MaxX, "#.0"); ","; FORMAT(bb.MaxY, "#.0");

  ' polilinea quasi retta: Douglas-Peucker con tol 0.5 lascia i due estremi
  DIM l(4) AS GeoPoint, o(4) AS GeoPoint
  l(0).X=0 : l(0).Y=0
  l(1).X=1 : l(1).Y=0.1
  l(2).X=2 : l(2).Y=0
  l(3).X=3 : l(3).Y=0
  l(4).X=4 : l(4).Y=0
  PRINT "|simp="; GEO.SIMPLIFY(l, 5, 0.5, o, 5); "="; FORMAT(o(1).X, "#.0"); ","; FORMAT(o(1).Y, "#.0");
  PRINT "|tr="; GEO.TRANSLATE(q, 4, 1, 2, o); "="; FORMAT(o(0).X, "#.0"); ","; FORMAT(o(0).Y, "#.0");
  PRINT "|sc="; GEO.SCALE(q, 4, 2, 3, o); "="; FORMAT(o(1).X, "#.0"); ","; FORMAT(o(2).Y, "#.0");
  PRINT "|ro="; GEO.ROTATE(q, 4, 90, o); "="; FORMAT(o(1).X, "#.0"); ","; FORMAT(o(1).Y, "#.0");

  ' clip: quadrato ∩ rettangolo (2,2)-(6,6) -> area 4
  DIM z(3) AS GeoPoint, k(7) AS GeoPoint
  z(0).X=2 : z(0).Y=2
  z(1).X=6 : z(1).Y=2
  z(2).X=6 : z(2).Y=6
  z(3).X=2 : z(3).Y=6
  PRINT "|clip="; GEO.CLIP(q, 4, z, 4, k, 8);
  PRINT ":"; FORMAT(GEO.POLYAREA(k, 4), "#.0");

  ' overlay: A=(0,0)-(6,6), B=(2,2)-(8,8) -> intersezione 16, unione 56, differenza 20
  DIM ax(3) AS GeoPoint, bx(3) AS GeoPoint, ox(15) AS GeoPoint
  ax(0).X=0 : ax(0).Y=0
  ax(1).X=6 : ax(1).Y=0
  ax(2).X=6 : ax(2).Y=6
  ax(3).X=0 : ax(3).Y=6
  bx(0).X=2 : bx(0).Y=2
  bx(1).X=8 : bx(1).Y=2
  bx(2).X=8 : bx(2).Y=8
  bx(3).X=2 : bx(3).Y=8
  DIM n AS LONG
  n = GEO.OVERLAY(ax, 4, bx, 4, ox, 16, 0)
  PRINT "|ov0="; n; ":"; FORMAT(GEO.POLYAREA(ox, n), "#.0");
  n = GEO.OVERLAY(ax, 4, bx, 4, ox, 16, 1)
  PRINT "|ov1="; n; ":"; FORMAT(GEO.POLYAREA(ox, n), "#.0");
  n = GEO.OVERLAY(ax, 4, bx, 4, ox, 16, 2)
  PRINT "|ov2="; n; ":"; FORMAT(GEO.POLYAREA(ox, n), "#.0");

  ' scafo convesso di un quadrato con punto interno
  DIM hq(4) AS GeoPoint, hh(7) AS GeoPoint
  hq(0).X=0 : hq(0).Y=0
  hq(1).X=4 : hq(1).Y=0
  hq(2).X=4 : hq(2).Y=4
  hq(3).X=0 : hq(3).Y=4
  hq(4).X=2 : hq(4).Y=2
  PRINT "|hu="; GEO.CONVEXHULL(hq, 5, hh, 8); ":"; FORMAT(GEO.POLYAREA(hh, 4), "#.0");
  PRINT "|f=0"
END SUB
