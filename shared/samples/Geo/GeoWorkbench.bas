' FILE: Geo/GeoWorkbench.bas
' SCENARIO DIDATTICO — ufficio tecnico comunale: due lotti, una zona edificabile,
' una strada e un punto di interesse. Il programma usa TUTTE le funzioni del
' namespace GEO su dati coerenti fra loro, così i numeri si possono verificare a mano.
'
' Impianto (metri, coordinate cartesiane):
'   Lotto A   rettangolo (0,0)-(10,6)
'   Lotto B   rettangolo (10,0)-(16,6)
'   Zona Z    rettangolo (4,1)-(12,5)  → interseca A e B
'   Strada    segmento da (-2,3) a (18,3)
'   POI       punto (5,4)
'
' Il percorso è quello di una pratica edilizia:
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
  DIM ax(3) AS DOUBLE, ay(3) AS DOUBLE
  ax(0)=0 : ax(1)=10 : ax(2)=10 : ax(3)=0
  ay(0)=0 : ay(1)=0 : ay(2)=6 : ay(3)=6
  PRINT "lotto-A area      = "; GEO.POLYAREA(ax, ay, 4)
  PRINT "lotto-A perimetro = "; GEO.PERIMETER(ax, ay, 4, 1)
  DIM c(1) AS DOUBLE
  GEO.CENTROID(ax, ay, 4, c)
  PRINT "lotto-A centroide = "; c(0); ","; c(1)

  ' Lo scafo convesso dei vertici (qui è il lotto stesso) e il bounding box.
  DIM hx(7) AS DOUBLE, hy(7) AS DOUBLE
  PRINT "lotto-A scafo     = "; GEO.CONVEXHULL(ax, ay, 4, hx, hy, 8)
  DIM bb(3) AS DOUBLE
  GEO.BBOX(ax, ay, 4, bb)
  PRINT "lotto-A bbox      = "; bb(0); ","; bb(1); " - "; bb(2); ","; bb(3)

  ' ── 2) vincoli: zona edificabile, strada, punto di interesse ────────────
  DIM zx(3) AS DOUBLE, zy(3) AS DOUBLE
  zx(0)=4 : zx(1)=12 : zx(2)=12 : zx(3)=4
  zy(0)=1 : zy(1)=1 : zy(2)=5 : zy(3)=5

  PRINT "POI in lotto A    = "; GEO.POINTINPOLY(5, 4, ax, ay, 4)
  PRINT "POI in zona Z     = "; GEO.POINTINPOLY(5, 4, zx, zy, 4)
  PRINT "POI sulla strada  = "; GEO.ONSEGMENT(5, 3, -2, 3, 18, 3)
  PRINT "POI-strada (m)    = "; GEO.DISTPOINTSEG(5, 4, -2, 3, 18, 3)
  PRINT "strada-zona (m)   = "; GEO.DISTSEGSEG(-2, 3, 18, 3, 4, 1, 12, 1)
  PRINT "POI-centroide (m) = "; GEO.DIST(5, 4, c(0), c(1))

  ' La strada attraversa il lato sud della zona? Punto di incidenza.
  DIM p(1) AS DOUBLE
  PRINT "strada x zona     = "; GEO.SEGINTERSECT(-2, 3, 18, 3, 4, 1, 4, 5)
  GEO.SEGINTERSECTION(p, -2, 3, 18, 3, 4, 1, 4, 5)
  PRINT "incidenza         = "; p(0); ","; p(1)

  ' ── 3) ritaglio e overlay: cosa è edificabile dentro A ──────────────────
  DIM kx(7) AS DOUBLE, ky(7) AS DOUBLE
  PRINT "A intersecato Z   = "; GEO.CLIP(ax, ay, 4, zx, zy, 4, kx, ky, 8)
  PRINT "area A in Z       = "; GEO.POLYAREA(kx, ky, 4)
  DIM ox(15) AS DOUBLE, oy(15) AS DOUBLE, n AS LONG
  n = GEO.OVERLAY(ax, ay, 4, zx, zy, 4, ox, oy, 16, 0)
  PRINT "overlay A n Z     = "; n; " area "; GEO.POLYAREA(ox, oy, n)
  n = GEO.OVERLAY(ax, ay, 4, zx, zy, 4, ox, oy, 16, 2)
  PRINT "overlay A meno Z  = "; n; " area "; GEO.POLYAREA(ox, oy, n)
  n = GEO.OVERLAY(ax, ay, 4, zx, zy, 4, ox, oy, 16, 1)
  PRINT "overlay A unito Z = "; n; " area "; GEO.POLYAREA(ox, oy, n)

  ' ── 4) rilievo da drone: semplifica e riporta in coordinate locali ──────
  DIM rx(5) AS DOUBLE, ry(5) AS DOUBLE
  rx(0)=0 : rx(1)=3 : rx(2)=6 : rx(3)=9 : rx(4)=10
  ry(0)=0 : ry(1)=0.2 : ry(2)=0 : ry(3)=0.1 : ry(4)=0
  DIM sx(5) AS DOUBLE, sy(5) AS DOUBLE
  PRINT "rilievo semplif.  = "; GEO.SIMPLIFY(rx, ry, 5, 0.5, sx, sy, 6)
  PRINT "traslazione       = "; GEO.TRANSLATE(ax, ay, 4, 100, 200, sx, sy)
  PRINT "        primo x,y = "; sx(0); ","; sy(0)
  PRINT "scala 2x          = "; GEO.SCALE(ax, ay, 4, 2, 2, sx, sy)
  PRINT "        lato est  = "; sx(1); ","; sy(2)
  PRINT "rotazione 90      = "; GEO.ROTATE(ax, ay, 4, 90, sx, sy)
  PRINT "        vertice   = "; sx(1); ","; sy(1)

  ' ── 5) georeferenziazione: due indirizzi in lon/lat ─────────────────────
  DIM m1(1) AS DOUBLE, m2(1) AS DOUBLE
  GEO.PROJECT(9.19, 45.4642, m1)     ' Milano, Duomo
  GEO.PROJECT(9.1916, 45.4655, m2)   ' a poca distanza
  PRINT "Duomo XY (m)      = "; m1(0); ","; m1(1)
  GEO.UNPROJECT(m1(0), m1(1), m2)
  PRINT "inverso lon/lat   = "; m2(0); ","; m2(1)
  PRINT "distanza (m)      = "; GEO.HAVERSINE(9.19, 45.4642, 9.1916, 45.4655)

  ' ── 6) scambio dati e ricerca per finestra ──────────────────────────────
  DIM gj AS STRING, wk AS STRING
  gj = GEO.GEOJSONWRITE(ax, ay, 4, 1)
  PRINT "GeoJSON lotto A   = "; gj
  DIM px(7) AS DOUBLE, py(7) AS DOUBLE, info(0) AS LONG
  PRINT "riletto vertici   = "; GEO.GEOJSONPARSE(gj, px, py, 8, info)
  PRINT "        chiuso    = "; info(0)
  wk = GEO.WKTWRITE(ax, ay, 4, 1)
  PRINT "WKT lotto A       = "; wk
  PRINT "riletto WKT       = "; GEO.WKTPARSE(wk, px, py, 8, info)
  DIM wpt(1) AS DOUBLE
  GEO.WKTPOINT("POINT (7 8)", wpt)
  PRINT "WKT point         = "; wpt(0); ","; wpt(1)

  ' Spatial join: quali lotti toccano quali zone? (A e B contro Z)
  DIM pax(7) AS DOUBLE, pay(7) AS DOUBLE, pbx(7) AS DOUBLE, pby(7) AS DOUBLE
  DIM aoff(2) AS LONG, boff(2) AS LONG, pairs(7) AS LONG
  DIM i AS LONG
  FOR i = 0 TO 3
    pax(i) = ax(i) : pay(i) = ay(i)
  NEXT i
  pax(4)=10 : pax(5)=16 : pax(6)=16 : pax(7)=10
  pay(4)=0 : pay(5)=0 : pay(6)=6 : pay(7)=6
  FOR i = 0 TO 3
    pbx(i) = zx(i) : pby(i) = zy(i)
  NEXT i
  ' seconda "zona" lontana, per mostrare che NON entra nel join
  pbx(4)=100 : pbx(5)=110 : pbx(6)=110 : pbx(7)=100
  pby(4)=100 : pby(5)=100 : pby(6)=110 : pby(7)=110
  aoff(0)=0 : aoff(1)=4 : aoff(2)=8
  boff(0)=0 : boff(1)=4 : boff(2)=8
  PRINT "coppie lotto/zona = "; GEO.SPATIALJOIN(pax, pay, aoff, pairs, pbx, pby, boff, 8, 2, 2)
  PRINT "        prima     = "; pairs(0); ","; pairs(1)

  ' Indice a griglia sui vertici dei due lotti: quali cadono nella finestra?
  DIM gx(7) AS DOUBLE, gy(7) AS DOUBLE, idx(7) AS LONG
  FOR i = 0 TO 3
    gx(i) = ax(i) : gy(i) = ay(i)
  NEXT i
  gx(4)=10 : gx(5)=16 : gx(6)=16 : gx(7)=10
  gy(4)=0 : gy(5)=0 : gy(6)=6 : gy(7)=6
  gh = GEO.GRIDNEW(gx, gy, 8, 1.0)
  PRINT "griglia vertici   = "; GEO.GRIDQUERY(gh, 0, 0, 0.5, 0.5, idx, 8); " -> "; idx(0)
  PRINT "griglia est       = "; GEO.GRIDQUERY(gh, 15, 0, 16, 0.5, idx, 8); " -> "; idx(0)
  PRINT "griglia libera    = "; GEO.GRIDFREE(gh)
END SUB
