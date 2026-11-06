' GEO (FFI) — proiezioni/geodesia, I/O GeoJSON e WKT, spatial join, indice a griglia.
' EXPECT: pr= 0 =0.0,5621521.5|up= 0 =0.0,45.0|hv=0.0,111319.5|gp= 1 =1.50,2.50: 0 |wp= 2 =3.0,4.0: 0 |wpt= 0 =7.0,8.0|gw= 49 :{"type":"LineString","coordinates":[[0,0],[3,4]]}|ww= 21 :LINESTRING (0 0, 3 4)|sj= 1 : 0 , 0 |gr= 7301 : 1 = 0 : 1 = 2 |gf= 0 |f=0
IMPORT GEO

SUB Main()
  DIM o(3) AS DOUBLE
  PRINT "pr="; GEO.PROJECT(0, 45, o); "="; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0");
  DIM px AS DOUBLE, py AS DOUBLE
  px = o(0) : py = o(1)
  PRINT "|up="; GEO.UNPROJECT(px, py, o); "="; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0");
  PRINT "|hv="; FORMAT(GEO.HAVERSINE(0, 0, 0, 0), "#.0"); ","; FORMAT(GEO.HAVERSINE(0, 0, 1, 0), "#.0"); "|";

  DIM gx(7) AS DOUBLE, gy(7) AS DOUBLE, info(0) AS LONG
  PRINT "gp="; GEO.GEOJSONPARSE("{""type"":""Point"",""coordinates"":[1.5,2.5]}", gx, gy, 8, info); "=";
  PRINT FORMAT(gx(0), "#.00"); ","; FORMAT(gy(0), "#.00"); ":"; info(0);
  PRINT "|wp="; GEO.WKTPARSE("LINESTRING (0 0, 3 4)", gx, gy, 8, info); "=";
  PRINT FORMAT(gx(1), "#.0"); ","; FORMAT(gy(1), "#.0"); ":"; info(0);
  PRINT "|wpt="; GEO.WKTPOINT("POINT (7 8)", o); "="; FORMAT(o(0), "#.0"); ","; FORMAT(o(1), "#.0"); "|";

  DIM js AS STRING, ws AS STRING
  js = GEO.GEOJSONWRITE(gx, gy, 2, 0)
  ws = GEO.WKTWRITE(gx, gy, 2, 0)
  PRINT "gw="; LEN(js); ":"; js; "|";
  PRINT "ww="; LEN(ws); ":"; ws; "|";

  ' spatial join: A = [(0,0)-(2,2), (10,10)-(12,12)]; B = [(1,1)-(3,3), (20,20)-(22,22)]
  DIM ax(7) AS DOUBLE, ay(7) AS DOUBLE, bx(7) AS DOUBLE, by(7) AS DOUBLE
  DIM aoff(2) AS LONG, boff(2) AS LONG, pairs(7) AS LONG
  ax(0)=0 : ax(1)=2 : ax(2)=2 : ax(3)=0 : ax(4)=10 : ax(5)=12 : ax(6)=12 : ax(7)=10
  ay(0)=0 : ay(1)=0 : ay(2)=2 : ay(3)=2 : ay(4)=10 : ay(5)=10 : ay(6)=12 : ay(7)=12
  bx(0)=1 : bx(1)=3 : bx(2)=3 : bx(3)=1 : bx(4)=20 : bx(5)=22 : bx(6)=22 : bx(7)=20
  by(0)=1 : by(1)=1 : by(2)=3 : by(3)=3 : by(4)=20 : by(5)=20 : by(6)=22 : by(7)=22
  aoff(0)=0 : aoff(1)=4 : aoff(2)=8
  boff(0)=0 : boff(1)=4 : boff(2)=8
  PRINT "sj="; GEO.SPATIALJOIN(ax, ay, aoff, pairs, bx, by, boff, 8, 2, 2);
  PRINT ":"; pairs(0); ","; pairs(1); "|";

  ' griglia: vertici del quadrato (0,0)(4,0)(4,4)(0,4), cella 1
  DIM qx(3) AS DOUBLE, qy(3) AS DOUBLE, idx(7) AS LONG
  qx(0)=0 : qx(1)=4 : qx(2)=4 : qx(3)=0
  qy(0)=0 : qy(1)=0 : qy(2)=4 : qy(3)=4
  gh = GEO.GRIDNEW(qx, qy, 4, 1.0)
  PRINT "gr="; gh; ":";
  PRINT GEO.GRIDQUERY(gh, 0, 0, 0.5, 0.5, idx, 8); "="; idx(0); ":";
  PRINT GEO.GRIDQUERY(gh, 3.5, 3.5, 4.5, 4.5, idx, 8); "="; idx(0);
  PRINT "|gf="; GEO.GRIDFREE(gh);
  PRINT "|f=0"
END SUB
