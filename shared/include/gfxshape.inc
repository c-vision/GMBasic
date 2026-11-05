' gfxshape.inc — namespace GFXSHAPE via FFI (libgmbgraphics: gmbsuite/gmbgraphics).
' Poligoni = array piatto x0,y0,x1,y1,…; n = NUMERO DI VALORI (pari) → n/2 vertici.
' Predicati: 1 = dentro, 0 = fuori. Le funzioni con out() ritornano 0 = ok, -1 = errore.
NAMESPACE GFXSHAPE

' Punto in un poligono (even-odd, come il ray-casting classico).
DECLARE FUNCTION POINTINPOLYGON LIB "gmbgraphics" ALIAS "ofx_gfx_point_in_polygon" (x AS DOUBLE, y AS DOUBLE, pts() AS DOUBLE, n AS LONG) AS LONG
' Punto in un cerchio (bordo incluso).
DECLARE FUNCTION POINTINCIRCLE LIB "gmbgraphics" ALIAS "ofx_gfx_point_in_circle" (x AS DOUBLE, y AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE, r AS DOUBLE) AS LONG
' Punto in un rettangolo (vertici in qualunque ordine).
DECLARE FUNCTION POINTINRECT LIB "gmbgraphics" ALIAS "ofx_gfx_point_in_rect" (x AS DOUBLE, y AS DOUBLE, x1 AS DOUBLE, y1 AS DOUBLE, x2 AS DOUBLE, y2 AS DOUBLE) AS LONG
' Punto in un triangolo (baricentrico).
DECLARE FUNCTION POINTINTRIANGLE LIB "gmbgraphics" ALIAS "ofx_gfx_point_in_triangle" (x AS DOUBLE, y AS DOUBLE, ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE) AS LONG
' Area del poligono (shoelace, sempre >= 0).
DECLARE FUNCTION POLYGONAREA LIB "gmbgraphics" ALIAS "ofx_gfx_polygon_area" (pts() AS DOUBLE, n AS LONG) AS DOUBLE
' Baricentro dell'area: out(0)=x, out(1)=y.
DECLARE FUNCTION POLYGONCENTROID LIB "gmbgraphics" ALIAS "ofx_gfx_polygon_centroid" (pts() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG
' Punto più vicino sul segmento (a,b): out(0)=x, out(1)=y.
DECLARE FUNCTION CLOSESTPOINTSEG LIB "gmbgraphics" ALIAS "ofx_gfx_closest_point_segment" (px AS DOUBLE, py AS DOUBLE, ax AS DOUBLE, ay AS DOUBLE, bx AS DOUBLE, by AS DOUBLE, out() AS DOUBLE) AS LONG
