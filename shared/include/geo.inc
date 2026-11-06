' geo.inc — namespace GEO: geometria piana / GIS (pacchetto gmbgeo, FFI).
'
' Geometrie = DUE array paralleli xs()/ys() + numero di vertici n (le struct C
' `geo_pt {x,y}` non attraversano l'FFI). Poligoni: vertici in ordine, senza
' ripetere il primo. I/O in GeoJSON e WKT; indice spaziale a griglia su handle.
'
' NON duplica la superficie che c'e' gia':
'   - distanze BATCH su matrici  -> SPATIAL.PDIST / CDIST / DISTANCEMATRIX
'   - inviluppo convesso          -> SPATIAL.CONVEXHULL (indici dei vertici)
'   - trigonometria (sin/cos)     -> MATH
'   - operazioni su matrici       -> MATRIX

NAMESPACE GEO

' ── misure ────────────────────────────────────────────────────────────────
' Area firmata (shoelace): positiva se il poligono e' in senso antiorario.
DECLARE FUNCTION POLYAREA LIB "gmbgeo" ALIAS "ofx_geo_poly_area" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG) AS DOUBLE
' Perimetro se closed <> 0, altrimenti lunghezza della polilinea aperta.
DECLARE FUNCTION PERIMETER LIB "gmbgeo" ALIAS "ofx_geo_perimeter" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, closed AS LONG) AS DOUBLE
' Centroide: area-ponderato se il poligono e' chiuso (n >= 3), altrimenti media dei vertici.
' out(0) = x, out(1) = y.
DECLARE FUNCTION CENTROID LIB "gmbgeo" ALIAS "ofx_geo_centroid" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG
' Distanza euclidea fra due punti (per le matrici di distanze vedi SPATIAL).
DECLARE FUNCTION DIST LIB "gmbgeo" ALIAS "ofx_geo_dist" (ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE) AS DOUBLE

' ── predicati e distanze ──────────────────────────────────────────────────
DECLARE FUNCTION POINTINPOLY LIB "gmbgeo" ALIAS "ofx_geo_point_in_poly" (px AS DOUBLE, py AS DOUBLE, xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG) AS LONG
DECLARE FUNCTION ONSEGMENT LIB "gmbgeo" ALIAS "ofx_geo_on_segment" (px AS DOUBLE, py AS DOUBLE, ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE) AS LONG
DECLARE FUNCTION DISTPOINTSEG LIB "gmbgeo" ALIAS "ofx_geo_dist_point_seg" (px AS DOUBLE, py AS DOUBLE, ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE) AS DOUBLE
DECLARE FUNCTION DISTSEGSEG LIB "gmbgeo" ALIAS "ofx_geo_dist_seg_seg" (ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE, dx AS DOUBLE, dy AS DOUBLE) AS DOUBLE

' ── intersezione fra segmenti ─────────────────────────────────────────────
DECLARE FUNCTION SEGINTERSECT LIB "gmbgeo" ALIAS "ofx_geo_seg_intersect" (ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE, dx AS DOUBLE, dy AS DOUBLE) AS LONG
' Punto di intersezione in out(0..1); ritorna 1 se esiste.
DECLARE FUNCTION SEGINTERSECTION LIB "gmbgeo" ALIAS "ofx_geo_seg_intersection" (out_() AS DOUBLE, ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE, dx AS DOUBLE, dy AS DOUBLE) AS LONG

' ── forma ─────────────────────────────────────────────────────────────────
' Bounding box: out(0)=minx out(1)=miny out(2)=maxx out(3)=maxy.
DECLARE FUNCTION BBOX LIB "gmbgeo" ALIAS "ofx_geo_bbox" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, out_() AS DOUBLE) AS LONG
' Douglas-Peucker: semplifica in oxs/oys (cap vertici), ritorna il numero di vertici.
DECLARE FUNCTION SIMPLIFY LIB "gmbgeo" ALIAS "ofx_geo_simplify" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, tol AS DOUBLE, oxs() AS DOUBLE, oys() AS DOUBLE, cap AS LONG) AS LONG
DECLARE FUNCTION TRANSLATE LIB "gmbgeo" ALIAS "ofx_geo_translate" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, dx AS DOUBLE, dy AS DOUBLE, oxs() AS DOUBLE, oys() AS DOUBLE) AS LONG
DECLARE FUNCTION SCALE LIB "gmbgeo" ALIAS "ofx_geo_scale" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, sx AS DOUBLE, sy AS DOUBLE, oxs() AS DOUBLE, oys() AS DOUBLE) AS LONG
' Rotazione attorno all'origine, gradi (senso antiorario).
DECLARE FUNCTION ROTATE LIB "gmbgeo" ALIAS "ofx_geo_rotate" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, deg AS DOUBLE, oxs() AS DOUBLE, oys() AS DOUBLE) AS LONG
' Ritaglio soggetto ∩ clip CONVESSO (Sutherland-Hodgman, clip in senso antiorario).
DECLARE FUNCTION CLIP LIB "gmbgeo" ALIAS "ofx_geo_clip" (sxs() AS DOUBLE, sys() AS DOUBLE, ns AS LONG, cxs() AS DOUBLE, cys() AS DOUBLE, nc AS LONG, oxs() AS DOUBLE, oys() AS DOUBLE, cap AS LONG) AS LONG

' ── proiezioni e geodesia ─────────────────────────────────────────────────
' Web Mercator: lon/lat in gradi -> out(0)=x out(1)=y in metri.
DECLARE FUNCTION PROJECT LIB "gmbgeo" ALIAS "ofx_geo_project" (lon AS DOUBLE, lat AS DOUBLE, out_() AS DOUBLE) AS LONG
' Inversa: out(0)=lon out(1)=lat (gradi).
DECLARE FUNCTION UNPROJECT LIB "gmbgeo" ALIAS "ofx_geo_unproject" (x AS DOUBLE, y AS DOUBLE, out_() AS DOUBLE) AS LONG
' Distanza geodesica in metri (formula sferica).
DECLARE FUNCTION HAVERSINE LIB "gmbgeo" ALIAS "ofx_geo_haversine" (lon1 AS DOUBLE, lat1 AS DOUBLE, lon2 AS DOUBLE, lat2 AS DOUBLE) AS DOUBLE

' ── overlay booleano di poligoni semplici (Greiner-Hormann) ────────────────
' op: 0 = intersezione, 1 = unione, 2 = differenza (A - B). Ritorna i vertici del
' risultato (0 = vuoto, -1 errore); poligoni non degeneri, anche non convessi.
DECLARE FUNCTION OVERLAY LIB "gmbgeo" ALIAS "ofx_geo_overlay" (axs() AS DOUBLE, ays() AS DOUBLE, na AS LONG, bxs() AS DOUBLE, bys() AS DOUBLE, nb AS LONG, oxs() AS DOUBLE, oys() AS DOUBLE, cap AS LONG, op AS LONG) AS LONG

' ── spatial join: coppie (i,j) di poligoni che si intersezionano ──────────
' aoff/boff = offset (na+1, nb+1) nei due array PIATTI: il poligono k e'
' [off(k) .. off(k+1)). pairs() riceve 2*count indici; ritorna il numero di coppie.
DECLARE FUNCTION SPATIALJOIN LIB "gmbgeo" ALIAS "ofx_geo_spatial_join" (axs() AS DOUBLE, ays() AS DOUBLE, aoff() AS LONG, pairs() AS LONG, bxs() AS DOUBLE, bys() AS DOUBLE, boff() AS LONG, cap AS LONG, na AS LONG, nb AS LONG) AS LONG

' ── I/O GeoJSON / WKT ─────────────────────────────────────────────────────
' Parse in oxs/oys (cap vertici); info(0) = 1 se il poligono e' chiuso. Ritorna n.
DECLARE FUNCTION GEOJSONPARSE LIB "gmbgeo" ALIAS "ofx_geo_geojson_parse" (json AS STRING, oxs() AS DOUBLE, oys() AS DOUBLE, cap AS LONG, info() AS LONG) AS LONG
DECLARE FUNCTION WKTPARSE LIB "gmbgeo" ALIAS "ofx_geo_wkt_parse" (wkt AS STRING, oxs() AS DOUBLE, oys() AS DOUBLE, cap AS LONG, info() AS LONG) AS LONG
' Scorciatoia POINT: out(0)=x out(1)=y.
DECLARE FUNCTION WKTPOINT LIB "gmbgeo" ALIAS "ofx_geo_wkt_point" (wkt AS STRING, out_() AS DOUBLE) AS LONG
' Serializzano (stringa gestita dal runtime: la copia avviene al ritorno).
DECLARE FUNCTION GEOJSONWRITE LIB "gmbgeo" ALIAS "ofx_geo_geojson_write" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, closed AS LONG) AS STRING
DECLARE FUNCTION WKTWRITE LIB "gmbgeo" ALIAS "ofx_geo_wkt_write" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, closed AS LONG) AS STRING

' ── indice spaziale a griglia uniforme — handle 7301+ ─────────────────────
DECLARE FUNCTION GRIDNEW LIB "gmbgeo" ALIAS "ofx_geo_grid_new" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, cell AS DOUBLE) AS LONG
' Indici dei punti nella finestra [minx..maxx] × [miny..maxy]; ritorna quanti.
DECLARE FUNCTION GRIDQUERY LIB "gmbgeo" ALIAS "ofx_geo_grid_query" (h AS LONG, minx AS DOUBLE, miny AS DOUBLE, maxx AS DOUBLE, maxy AS DOUBLE, idx() AS LONG, cap AS LONG) AS LONG
DECLARE FUNCTION GRIDFREE LIB "gmbgeo" ALIAS "ofx_geo_grid_free" (h AS LONG) AS LONG

' ── Inviluppo convesso ────────────────────────────────────────────────────
' Ritorna i vertici dello scafo in oxs/oys (n. vertici, 0/-1 errore) — pronto per
' POLYAREA/OVERLAY. SPATIAL.CONVEXHULL resta la variante che restituisce gli INDICI
' dei vertici su una matrice X/Y (per selezionare punti da un array esistente).
DECLARE FUNCTION CONVEXHULL LIB "gmbgeo" ALIAS "ofx_geo_convex_hull" (xs() AS DOUBLE, ys() AS DOUBLE, n AS LONG, oxs() AS DOUBLE, oys() AS DOUBLE, cap AS LONG) AS LONG
