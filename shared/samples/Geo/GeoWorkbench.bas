' FILE: Geo/GeoWorkbench.bas
' SCENARIO DIDATTICO — ufficio tecnico comunale: due lotti, una zona edificabile,
' una strada e un punto di interesse. Il programma usa TUTTE le funzioni del
' namespace GEO su dati coerenti fra loro, così i numeri si possono verificare a mano.
'
' Geometrie: array del TYPE GeoPoint { X, Y AS DOUBLE } — lo stesso layout della
' struct C, quindi il buffer viaggia contiguo e i punti tornano per valore.
'
' Impianto (metri, coordinate cartesiane):
'   Lotto A   rettangolo (0,0)-(10,6)
'   Lotto B   rettangolo (10,0)-(16,6)
'   Zona Z    rettangolo (4,1)-(12,5)  → interseca A e B
'   Strada    segmento da (-2,3) a (18,3)
'   POI       punto (5,4)
'
' Percorso di una pratica edilizia:
'   1) misure del lotto (area, perimetro, centroide, scafo, bbox)
'   2) vincoli geometrici (zona, strada, punto interno)
'   3) ritaglio/overlay: quanta parte di A cade in Z, e cosa resta fuori
'   4) semplificazione e trasformazioni (il rilievo arriva da un drone)
'   5) georeferenziazione (Web Mercator + haversine fra due indirizzi)
'   6) scambio dati (GeoJSON/WKT) e indice spaziale per la ricerca per finestra
'
' Domini: GEO. Output deterministico.
IMPORT GEO

SUB Main()
  ' ── 1) il lotto A ─────────────────────────────────────────────────────────
  DIM a(3) AS GeoPoint
  a(0).X=0  : a(0).Y=0
  a(1).X=10 : a(1).Y=0
  a(2).X=10 : a(2).Y=6
  a(3).X=0  : a(3).Y=6
  PRINT "lotto-A area      = "; GEO.POLYAREA(a, 4)
  PRINT "lotto-A perimetro = "; GEO.PERIMETER(a, 4, 1)
  DIM c AS GeoPoint
  c = GEO.CENTROID(a, 4)
  PRINT "lotto-A centroide = "; c.X; ","; c.Y

  DIM h(7) AS GeoPoint
  PRINT "lotto-A scafo     = "; GEO.CONVEXHULL(a, 4, h, 8)
  DIM bb AS GeoBox
  bb = GEO.BBOX(a, 4)
  PRINT "lotto-A bbox      = "; bb.MinX; ","; bb.MinY; " - "; bb.MaxX; ","; bb.MaxY

  ' ── 2) vincoli: zona edificabile, strada, punto di interesse ────────────
  DIM z(3) AS GeoPoint
  z(0).X=4  : z(0).Y=1
  z(1).X=12 : z(1).Y=1
  z(2).X=12 : z(2).Y=5
  z(3).X=4  : z(3).Y=5

  DIM poi AS GeoPoint, sa AS GeoPoint, sb AS GeoPoint
  poi.X=5 : poi.Y=4
  sa.X=-2 : sa.Y=3
  sb.X=18 : sb.Y=3
  PRINT "POI in lotto A    = "; GEO.POINTINPOLY(poi, a, 4)
  PRINT "POI in zona Z     = "; GEO.POINTINPOLY(poi, z, 4)
  PRINT "POI sulla strada  = "; GEO.ONSEGMENT(poi, sa, sb)
  PRINT "POI-strada (m)    = "; GEO.DISTPOINTSEG(poi, sa, sb)
  DIM zl0 AS GeoPoint, zl1 AS GeoPoint
  zl0.X=4 : zl0.Y=1
  zl1.X=12 : zl1.Y=1
  PRINT "strada-zona (m)   = "; GEO.DISTSEGSEG(sa, sb, zl0, zl1)
  PRINT "POI-centroide (m) = "; GEO.DIST(poi, c)

  ' la strada attraversa il lato ovest della zona?
  DIM zw0 AS GeoPoint, zw1 AS GeoPoint, ip(0) AS GeoPoint
  zw0.X=4 : zw0.Y=1
  zw1.X=4 : zw1.Y=5
  PRINT "strada x zona     = "; GEO.SEGINTERSECT(sa, sb, zw0, zw1)
  PRINT "incidenza         = "; GEO.SEGINTERSECTION(sa, sb, zw0, zw1, ip); ": "; ip(0).X; ","; ip(0).Y

  ' ── 3) ritaglio e overlay: cosa è edificabile dentro A ──────────────────
  DIM k(7) AS GeoPoint, o(15) AS GeoPoint, n AS LONG
  PRINT "A intersecato Z   = "; GEO.CLIP(a, 4, z, 4, k, 8)
  PRINT "area A in Z       = "; GEO.POLYAREA(k, 4)
  n = GEO.OVERLAY(a, 4, z, 4, o, 16, 0)
  PRINT "overlay A n Z     = "; n; " area "; GEO.POLYAREA(o, n)
  n = GEO.OVERLAY(a, 4, z, 4, o, 16, 2)
  PRINT "overlay A meno Z  = "; n; " area "; GEO.POLYAREA(o, n)
  n = GEO.OVERLAY(a, 4, z, 4, o, 16, 1)
  PRINT "overlay A unito Z = "; n; " area "; GEO.POLYAREA(o, n)

  ' ── 4) rilievo da drone: semplifica e riporta in coordinate locali ──────
  DIM r(4) AS GeoPoint, s(4) AS GeoPoint
  r(0).X=0  : r(0).Y=0
  r(1).X=3  : r(1).Y=0.2
  r(2).X=6  : r(2).Y=0
  r(3).X=9  : r(3).Y=0.1
  r(4).X=10 : r(4).Y=0
  PRINT "rilievo semplif.  = "; GEO.SIMPLIFY(r, 5, 0.5, s, 6)
  PRINT "traslazione       = "; GEO.TRANSLATE(a, 4, 100, 200, s)
  PRINT "        primo x,y = "; s(0).X; ","; s(0).Y
  PRINT "scala 2x          = "; GEO.SCALE(a, 4, 2, 2, s)
  PRINT "        lato est  = "; s(1).X; ","; s(2).Y
  PRINT "rotazione 90      = "; GEO.ROTATE(a, 4, 90, s)
  PRINT "        vertice   = "; s(1).X; ","; s(1).Y

  ' ── 5) georeferenziazione: due indirizzi in lon/lat ─────────────────────
  DIM m1 AS GeoPoint, m2 AS GeoPoint
  m1 = GEO.PROJECT(9.19, 45.4642)        ' Milano, Duomo
  PRINT "Duomo XY (m)      = "; m1.X; ","; m1.Y
  m2 = GEO.UNPROJECT(m1.X, m1.Y)
  PRINT "inverso lon/lat   = "; m2.X; ","; m2.Y
  PRINT "distanza (m)      = "; GEO.HAVERSINE(9.19, 45.4642, 9.1916, 45.4655)

  ' ── 6) scambio dati e ricerca per finestra ──────────────────────────────
  DIM gj AS STRING, wk AS STRING
  gj = GEO.GEOJSONWRITE(a, 4, 1)
  PRINT "GeoJSON lotto A   = "; gj
  DIM px(7) AS GeoPoint, info(0) AS LONG
  PRINT "riletto vertici   = "; GEO.GEOJSONPARSE(gj, px, 8, info)
  PRINT "        chiuso    = "; info(0)
  wk = GEO.WKTWRITE(a, 4, 1)
  PRINT "WKT lotto A       = "; wk
  PRINT "riletto WKT       = "; GEO.WKTPARSE(wk, px, 8, info)
  DIM wp AS GeoPoint
  wp = GEO.WKTPOINT("POINT (7 8)")
  PRINT "WKT point         = "; wp.X; ","; wp.Y

  ' Spatial join: quali lotti toccano quali zone? (A e B contro Z e una zona lontana)
  DIM pa(7) AS GeoPoint, pz(7) AS GeoPoint
  DIM aoff(2) AS LONG, boff(2) AS LONG, pairs(7) AS LONG
  DIM i AS LONG
  FOR i = 0 TO 3
    pa(i).X = a(i).X : pa(i).Y = a(i).Y
  NEXT i
  pa(4).X=10 : pa(4).Y=0
  pa(5).X=16 : pa(5).Y=0
  pa(6).X=16 : pa(6).Y=6
  pa(7).X=10 : pa(7).Y=6
  FOR i = 0 TO 3
    pz(i).X = z(i).X : pz(i).Y = z(i).Y
  NEXT i
  pz(4).X=100 : pz(4).Y=100
  pz(5).X=110 : pz(5).Y=100
  pz(6).X=110 : pz(6).Y=110
  pz(7).X=100 : pz(7).Y=110
  aoff(0)=0 : aoff(1)=4 : aoff(2)=8
  boff(0)=0 : boff(1)=4 : boff(2)=8
  PRINT "coppie lotto/zona = "; GEO.SPATIALJOIN(pa, aoff, 2, pz, boff, 2, pairs, 8)
  PRINT "        prima     = "; pairs(0); ","; pairs(1)

  ' Indice a griglia sui vertici dei due lotti: quali cadono nella finestra?
  DIM g(7) AS GeoPoint, idx(7) AS LONG, win AS GeoBox
  FOR i = 0 TO 3
    g(i).X = a(i).X : g(i).Y = a(i).Y
  NEXT i
  g(4).X=10 : g(4).Y=0
  g(5).X=16 : g(5).Y=0
  g(6).X=16 : g(6).Y=6
  g(7).X=10 : g(7).Y=6
  gh = GEO.GRIDNEW(g, 8, 1.0)
  win.MinX=0 : win.MinY=0 : win.MaxX=0.5 : win.MaxY=0.5
  PRINT "griglia vertici   = "; GEO.GRIDQUERY(gh, win, idx, 8); " -> "; idx(0)
  win.MinX=15 : win.MinY=0 : win.MaxX=16 : win.MaxY=0.5
  PRINT "griglia est       = "; GEO.GRIDQUERY(gh, win, idx, 8); " -> "; idx(0)
  PRINT "griglia libera    = "; GEO.GRIDFREE(gh)
END SUB
