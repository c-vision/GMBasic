' geo.inc — namespace GEO: geometria piana / GIS (pacchetto gmbgeo, FFI).
'
' Una geometria è un ARRAY di `GeoPoint` + il numero di vertici n. Il layout del
' TYPE (due double) è identico a quello della struct C `geo_pt`: il buffer viaggia
' contiguo, zero-copy in ingresso e copy-out in uscita. Le funzioni che restituiscono
' un punto lo fanno per valore (GeoPoint); il bounding box torna come `GeoBox`.
'
' Poligoni: vertici in ordine, senza ripetere il primo; l'ordine antiorario dà area
' positiva. Gli array di uscita sono forniti dal chiamante; `cap` è il numero massimo
' di vertici scrivibili.
'
' NON duplica la superficie che c'è già:
'   - distanze BATCH su matrici  -> SPATIAL.PDIST / CDIST / DISTANCEMATRIX
'   - indici dei vertici dello scafo -> SPATIAL.CONVEXHULL (qui il poligono)
'   - trigonometria              -> MATH
'   - operazioni su matrici      -> MATRIX

NAMESPACE GEO

TYPE GeoPoint
  X AS DOUBLE
  Y AS DOUBLE
END TYPE

TYPE GeoBox
  MinX AS DOUBLE
  MinY AS DOUBLE
  MaxX AS DOUBLE
  MaxY AS DOUBLE
END TYPE

' ── misure ────────────────────────────────────────────────────────────────
' Area firmata (shoelace): positiva se il poligono è in senso antiorario.
DECLARE FUNCTION POLYAREA LIB "gmbgeo" ALIAS "ofx_geo_poly_area" (p() AS GeoPoint, n AS LONG) AS DOUBLE
' Perimetro se closed <> 0, altrimenti lunghezza della polilinea aperta.
DECLARE FUNCTION PERIMETER LIB "gmbgeo" ALIAS "ofx_geo_perimeter" (p() AS GeoPoint, n AS LONG, closed AS LONG) AS DOUBLE
' Centroide per valore: area-ponderato se il poligono è chiuso (n >= 3), altrimenti
' media dei vertici.
DECLARE FUNCTION CENTROID LIB "gmbgeo" ALIAS "ofx_geo_centroid" (p() AS GeoPoint, n AS LONG) AS GeoPoint
' Distanza euclidea fra due punti (per le matrici di distanze vedi SPATIAL).
DECLARE FUNCTION DIST LIB "gmbgeo" ALIAS "ofx_geo_dist" (a AS GeoPoint, b AS GeoPoint) AS DOUBLE

' ── predicati e distanze ──────────────────────────────────────────────────
DECLARE FUNCTION POINTINPOLY LIB "gmbgeo" ALIAS "ofx_geo_point_in_poly" (q AS GeoPoint, p() AS GeoPoint, n AS LONG) AS LONG
DECLARE FUNCTION ONSEGMENT LIB "gmbgeo" ALIAS "ofx_geo_on_segment" (q AS GeoPoint, a AS GeoPoint, b AS GeoPoint) AS LONG
DECLARE FUNCTION DISTPOINTSEG LIB "gmbgeo" ALIAS "ofx_geo_dist_point_seg" (q AS GeoPoint, a AS GeoPoint, b AS GeoPoint) AS DOUBLE
DECLARE FUNCTION DISTSEGSEG LIB "gmbgeo" ALIAS "ofx_geo_dist_seg_seg" (a AS GeoPoint, b AS GeoPoint, c AS GeoPoint, d AS GeoPoint) AS DOUBLE

' ── intersezione fra segmenti ─────────────────────────────────────────────
DECLARE FUNCTION SEGINTERSECT LIB "gmbgeo" ALIAS "ofx_geo_seg_intersect" (a AS GeoPoint, b AS GeoPoint, c AS GeoPoint, d AS GeoPoint) AS LONG
' Punto di intersezione in out(0) (array di 1 GeoPoint); ritorna 1 se esiste.
DECLARE FUNCTION SEGINTERSECTION LIB "gmbgeo" ALIAS "ofx_geo_seg_intersection" (a AS GeoPoint, b AS GeoPoint, c AS GeoPoint, d AS GeoPoint, out_() AS GeoPoint) AS LONG

' ── forma ─────────────────────────────────────────────────────────────────
' Inviluppo convesso (monotone chain): scrive i vertici in out(), ritorna quanti.
DECLARE FUNCTION CONVEXHULL LIB "gmbgeo" ALIAS "ofx_geo_convex_hull" (p() AS GeoPoint, n AS LONG, out_() AS GeoPoint, cap AS LONG) AS LONG
' Bounding box per valore.
DECLARE FUNCTION BBOX LIB "gmbgeo" ALIAS "ofx_geo_bbox" (p() AS GeoPoint, n AS LONG) AS GeoBox
' Douglas-Peucker: semplifica in out() (cap vertici), ritorna il numero di vertici.
DECLARE FUNCTION SIMPLIFY LIB "gmbgeo" ALIAS "ofx_geo_simplify" (p() AS GeoPoint, n AS LONG, tol AS DOUBLE, out_() AS GeoPoint, cap AS LONG) AS LONG
DECLARE FUNCTION TRANSLATE LIB "gmbgeo" ALIAS "ofx_geo_translate" (p() AS GeoPoint, n AS LONG, dx AS DOUBLE, dy AS DOUBLE, out_() AS GeoPoint) AS LONG
DECLARE FUNCTION SCALE LIB "gmbgeo" ALIAS "ofx_geo_scale" (p() AS GeoPoint, n AS LONG, sx AS DOUBLE, sy AS DOUBLE, out_() AS GeoPoint) AS LONG
' Rotazione attorno all'origine, gradi (senso antiorario).
DECLARE FUNCTION ROTATE LIB "gmbgeo" ALIAS "ofx_geo_rotate" (p() AS GeoPoint, n AS LONG, deg AS DOUBLE, out_() AS GeoPoint) AS LONG
' Ritaglio soggetto ∩ clip CONVESSO (Sutherland-Hodgman, clip antiorario).
DECLARE FUNCTION CLIP LIB "gmbgeo" ALIAS "ofx_geo_clip" (subj() AS GeoPoint, ns AS LONG, clip() AS GeoPoint, nc AS LONG, out_() AS GeoPoint, cap AS LONG) AS LONG

' ── proiezioni e geodesia ─────────────────────────────────────────────────
' Web Mercator: GeoPoint(X = est metri, Y = nord metri) da lon/lat in gradi.
DECLARE FUNCTION PROJECT LIB "gmbgeo" ALIAS "ofx_geo_project" (lon AS DOUBLE, lat AS DOUBLE) AS GeoPoint
' Inversa: GeoPoint(X = lon, Y = lat) in gradi.
DECLARE FUNCTION UNPROJECT LIB "gmbgeo" ALIAS "ofx_geo_unproject" (x AS DOUBLE, y AS DOUBLE) AS GeoPoint
' Distanza geodesica in metri (sfera, R = 6378137).
DECLARE FUNCTION HAVERSINE LIB "gmbgeo" ALIAS "ofx_geo_haversine" (lon1 AS DOUBLE, lat1 AS DOUBLE, lon2 AS DOUBLE, lat2 AS DOUBLE) AS DOUBLE

' ── overlay booleano di poligoni semplici (Greiner-Hormann) ────────────────
' op: 0 = intersezione, 1 = unione, 2 = differenza (A - B). Ritorna i vertici del
' risultato (0 = vuoto, -1 errore); poligoni non degeneri, anche non convessi.
DECLARE FUNCTION OVERLAY LIB "gmbgeo" ALIAS "ofx_geo_overlay" (a() AS GeoPoint, na AS LONG, b() AS GeoPoint, nb AS LONG, out_() AS GeoPoint, cap AS LONG, op AS LONG) AS LONG

' ── spatial join: coppie (i,j) di poligoni che si intersezionano ──────────
' aoff/boff = offset (na+1, nb+1) nell'array piatto di GeoPoint: il poligono k è
' [off(k) .. off(k+1)). pairs() riceve 2*count indici; ritorna il numero di coppie.
DECLARE FUNCTION SPATIALJOIN LIB "gmbgeo" ALIAS "ofx_geo_spatial_join" (a() AS GeoPoint, aoff() AS LONG, na AS LONG, b() AS GeoPoint, boff() AS LONG, nb AS LONG, pairs() AS LONG, cap AS LONG) AS LONG

' ── I/O GeoJSON / WKT ─────────────────────────────────────────────────────
' Parse in out() (cap vertici); info(0) = 1 se il poligono è chiuso. Ritorna n.
DECLARE FUNCTION GEOJSONPARSE LIB "gmbgeo" ALIAS "ofx_geo_geojson_parse" (json AS STRING, out_() AS GeoPoint, cap AS LONG, info() AS LONG) AS LONG
DECLARE FUNCTION WKTPARSE LIB "gmbgeo" ALIAS "ofx_geo_wkt_parse" (wkt AS STRING, out_() AS GeoPoint, cap AS LONG, info() AS LONG) AS LONG
' Scorciatoia POINT, per valore.
DECLARE FUNCTION WKTPOINT LIB "gmbgeo" ALIAS "ofx_geo_wkt_point" (wkt AS STRING) AS GeoPoint
' Serializzano (stringa gestita dal runtime: la copia avviene al ritorno).
DECLARE FUNCTION GEOJSONWRITE LIB "gmbgeo" ALIAS "ofx_geo_geojson_write" (p() AS GeoPoint, n AS LONG, closed AS LONG) AS STRING
DECLARE FUNCTION WKTWRITE LIB "gmbgeo" ALIAS "ofx_geo_wkt_write" (p() AS GeoPoint, n AS LONG, closed AS LONG) AS STRING

' ── indice spaziale a griglia uniforme — handle 7301+ ─────────────────────
DECLARE FUNCTION GRIDNEW LIB "gmbgeo" ALIAS "ofx_geo_grid_new" (p() AS GeoPoint, n AS LONG, cell AS DOUBLE) AS LONG
' Indici dei punti nella finestra `box`; ritorna quanti.
DECLARE FUNCTION GRIDQUERY LIB "gmbgeo" ALIAS "ofx_geo_grid_query" (h AS LONG, box AS GeoBox, idx() AS LONG, cap AS LONG) AS LONG
DECLARE FUNCTION GRIDFREE LIB "gmbgeo" ALIAS "ofx_geo_grid_free" (h AS LONG) AS LONG
