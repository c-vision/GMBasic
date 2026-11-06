' GEO (FFI) — proiezioni/geodesia, I/O GeoJSON e WKT, spatial join, indice a griglia.
' EXPECT: pr=0.0,5621521.5|up=0.0,45.0|hv=0.0,111319.5|gp= 1 =1.50,2.50: 0 |wp= 2 =3.0,4.0: 0 |wpt=7.0,8.0|gw= 49 :{"type":"LineString","coordinates":[[0,0],[3,4]]}|ww= 21 :LINESTRING (0 0, 3 4)|sj= 1 : 0 , 0 |gr= 7301 : 1 = 0 : 1 = 2 |gf= 0 |f=0
IMPORT GEO

SUB Main()
  DIM m AS GeoPoint, inv AS GeoPoint
  m = GEO.PROJECT(0, 45)
  PRINT "pr="; FORMAT(m.X, "#.0"); ","; FORMAT(m.Y, "#.0");
  inv = GEO.UNPROJECT(m.X, m.Y)
  PRINT "|up="; FORMAT(inv.X, "#.0"); ","; FORMAT(inv.Y, "#.0");
  PRINT "|hv="; FORMAT(GEO.HAVERSINE(0, 0, 0, 0), "#.0"); ","; FORMAT(GEO.HAVERSINE(0, 0, 1, 0), "#.0"); "|";

  DIM g(7) AS GeoPoint, info(0) AS LONG
  PRINT "gp="; GEO.GEOJSONPARSE("{""type"":""Point"",""coordinates"":[1.5,2.5]}", g, 8, info); "=";
  PRINT FORMAT(g(0).X, "#.00"); ","; FORMAT(g(0).Y, "#.00"); ":"; info(0);
  PRINT "|wp="; GEO.WKTPARSE("LINESTRING (0 0, 3 4)", g, 8, info); "=";
  PRINT FORMAT(g(1).X, "#.0"); ","; FORMAT(g(1).Y, "#.0"); ":"; info(0);
  DIM f AS GeoPoint
  f = GEO.WKTPOINT("POINT (7 8)")
  PRINT "|wpt="; FORMAT(f.X, "#.0"); ","; FORMAT(f.Y, "#.0"); "|";

  DIM js AS STRING, ws AS STRING
  js = GEO.GEOJSONWRITE(g, 2, 0)
  ws = GEO.WKTWRITE(g, 2, 0)
  PRINT "gw="; LEN(js); ":"; js; "|";
  PRINT "ww="; LEN(ws); ":"; ws; "|";

  ' spatial join: A = [(0,0)-(2,2), (10,10)-(12,12)]; B = [(1,1)-(3,3), (20,20)-(22,22)]
  DIM a(7) AS GeoPoint, b(7) AS GeoPoint
  DIM aoff(2) AS LONG, boff(2) AS LONG, pairs(7) AS LONG
  a(0).X=0 : a(0).Y=0
  a(1).X=2 : a(1).Y=0
  a(2).X=2 : a(2).Y=2
  a(3).X=0 : a(3).Y=2
  a(4).X=10 : a(4).Y=10
  a(5).X=12 : a(5).Y=10
  a(6).X=12 : a(6).Y=12
  a(7).X=10 : a(7).Y=12
  b(0).X=1 : b(0).Y=1
  b(1).X=3 : b(1).Y=1
  b(2).X=3 : b(2).Y=3
  b(3).X=1 : b(3).Y=3
  b(4).X=20 : b(4).Y=20
  b(5).X=22 : b(5).Y=20
  b(6).X=22 : b(6).Y=22
  b(7).X=20 : b(7).Y=22
  aoff(0)=0 : aoff(1)=4 : aoff(2)=8
  boff(0)=0 : boff(1)=4 : boff(2)=8
  PRINT "sj="; GEO.SPATIALJOIN(a, aoff, 2, b, boff, 2, pairs, 8);
  PRINT ":"; pairs(0); ","; pairs(1); "|";

  ' griglia: vertici del quadrato (0,0)(4,0)(4,4)(0,4), cella 1
  DIM q(3) AS GeoPoint, idx(7) AS LONG
  q(0).X=0 : q(0).Y=0
  q(1).X=4 : q(1).Y=0
  q(2).X=4 : q(2).Y=4
  q(3).X=0 : q(3).Y=4
  gh = GEO.GRIDNEW(q, 4, 1.0)
  DIM win AS GeoBox
  win.MinX=0 : win.MinY=0 : win.MaxX=0.5 : win.MaxY=0.5
  PRINT "gr="; gh; ":";
  PRINT GEO.GRIDQUERY(gh, win, idx, 8); "="; idx(0); ":";
  win.MinX=3.5 : win.MinY=3.5 : win.MaxX=4.5 : win.MaxY=4.5
  PRINT GEO.GRIDQUERY(gh, win, idx, 8); "="; idx(0);
  PRINT "|gf="; GEO.GRIDFREE(gh);
  PRINT "|f=0"
END SUB
