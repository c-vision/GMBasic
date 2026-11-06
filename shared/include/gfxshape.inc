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

' ── AABB 2D ───────────────────────────────────────────────────────────────
' AABB = [minx,miny,maxx,maxy]; n = numero di VALORI del poligono/point-cloud.
DECLARE FUNCTION AABBFROMPOINTS2D LIB "gmbgraphics" ALIAS "ofx_gfx_shape_aabb_from_points" (pts() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AABBMERGE2D LIB "gmbgraphics" ALIAS "ofx_gfx_shape_aabb_merge" (a() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AABBCENTER2D LIB "gmbgraphics" ALIAS "ofx_gfx_shape_aabb_center" (a() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AABBEXTENTS2D LIB "gmbgraphics" ALIAS "ofx_gfx_shape_aabb_extents" (a() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION AABBVALID LIB "gmbgraphics" ALIAS "ofx_gfx_shape_aabb_valid" (a() AS DOUBLE) AS LONG

' ── vicinanza / distanze ──────────────────────────────────────────────────
DECLARE FUNCTION DIST LIB "gmbgraphics" ALIAS "ofx_gfx_shape_dist" (a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE
' Punto più vicino sul raggio 2D [ox,oy,dx,dy] / sull'AABB / su un cerchio / fra due AABB.
DECLARE FUNCTION CLOSESTRAY LIB "gmbgraphics" ALIAS "ofx_gfx_shape_closest_ray" (ray() AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION CLOSESTAABB LIB "gmbgraphics" ALIAS "ofx_gfx_shape_closest_aabb" (box() AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION CLOSESTCIRCLE LIB "gmbgraphics" ALIAS "ofx_gfx_shape_closest_circle" (cx AS DOUBLE, cy AS DOUBLE, r AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION CLOSESTAABBAABB LIB "gmbgraphics" ALIAS "ofx_gfx_shape_closest_aabb_aabb" (a() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE) AS LONG

' ── intersezioni 2D ───────────────────────────────────────────────────────
DECLARE FUNCTION CIRCLECIRCLE LIB "gmbgraphics" ALIAS "ofx_gfx_shape_circle_circle" (c1() AS DOUBLE, r1 AS DOUBLE, c2() AS DOUBLE, r2 AS DOUBLE) AS LONG
DECLARE FUNCTION AABBAABB LIB "gmbgraphics" ALIAS "ofx_gfx_shape_aabb_aabb" (a() AS DOUBLE, b() AS DOUBLE) AS LONG
DECLARE FUNCTION AABBCIRCLE LIB "gmbgraphics" ALIAS "ofx_gfx_shape_aabb_circle" (a() AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE, r AS DOUBLE) AS LONG
' Raggio 2D vs cerchio/AABB/segmento: t() riceve il parametro (o tmin/tmax per AABB).
DECLARE FUNCTION RAY2CIRCLE LIB "gmbgraphics" ALIAS "ofx_gfx_shape_ray2_circle" (ray() AS DOUBLE, cx AS DOUBLE, cy AS DOUBLE, r AS DOUBLE, t() AS DOUBLE) AS LONG
DECLARE FUNCTION RAY2AABB LIB "gmbgraphics" ALIAS "ofx_gfx_shape_ray2_aabb" (ray() AS DOUBLE, box() AS DOUBLE, t() AS DOUBLE) AS LONG
DECLARE FUNCTION RAY2SEGMENT LIB "gmbgraphics" ALIAS "ofx_gfx_shape_ray2_segment" (ray() AS DOUBLE, a() AS DOUBLE, b() AS DOUBLE, t() AS DOUBLE) AS LONG

' ── poligoni avanzati ─────────────────────────────────────────────────────
' Baricentro: out(0..2)=wa,wb,wc (somma 1); ritorno 0 su triangolo degenere.
DECLARE FUNCTION BARYCENTRIC LIB "gmbgraphics" ALIAS "ofx_gfx_shape_barycentric" (a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, p() AS DOUBLE, out() AS DOUBLE) AS LONG
' Rettangolo orientato (centro, assi u/v ortonormali, semi-lati) e quadrilatero (4 vertici).
DECLARE FUNCTION POINTINORIENTEDRECT LIB "gmbgraphics" ALIAS "ofx_gfx_shape_point_in_oriented_rect" (center() AS DOUBLE, u() AS DOUBLE, v() AS DOUBLE, half_u AS DOUBLE, half_v AS DOUBLE, p() AS DOUBLE) AS LONG
DECLARE FUNCTION POINTINQUAD LIB "gmbgraphics" ALIAS "ofx_gfx_shape_point_in_quad" (a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, d() AS DOUBLE, p() AS DOUBLE) AS LONG
DECLARE FUNCTION POLYGONSIGNEDAREA LIB "gmbgraphics" ALIAS "ofx_gfx_shape_polygon_signed_area" (pts() AS DOUBLE, n AS LONG) AS DOUBLE

' ── esagono regolare ──────────────────────────────────────────────────────
DECLARE FUNCTION HEXAREA LIB "gmbgraphics" ALIAS "ofx_gfx_shape_hex_area" (side AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HEXLONG LIB "gmbgraphics" ALIAS "ofx_gfx_shape_hex_long" (side AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HEXSHORT LIB "gmbgraphics" ALIAS "ofx_gfx_shape_hex_short" (side AS DOUBLE) AS DOUBLE
DECLARE FUNCTION HEXINNER LIB "gmbgraphics" ALIAS "ofx_gfx_shape_hex_inner" (side AS DOUBLE) AS DOUBLE

' ── point cloud 3D ────────────────────────────────────────────────────────
' A/B = array piatti x,y,z,…; n = numero di VALORI (multipli di 3).
DECLARE FUNCTION CHAMFER LIB "gmbgraphics" ALIAS "ofx_gfx_shape_chamfer" (A() AS DOUBLE, n AS LONG, B() AS DOUBLE, m AS LONG) AS DOUBLE
DECLARE FUNCTION HAUSDORFF LIB "gmbgraphics" ALIAS "ofx_gfx_shape_hausdorff" (A() AS DOUBLE, n AS LONG, B() AS DOUBLE, m AS LONG) AS DOUBLE

' Costruttori 2D (AABB da min/max, punto al parametro t sul raggio).
DECLARE FUNCTION AABB2MAKE LIB "gmbgraphics" ALIAS "ofx_gfx_shape_aabb2_make" (mn() AS DOUBLE, mx() AS DOUBLE, out() AS DOUBLE) AS LONG
DECLARE FUNCTION RAY2AT LIB "gmbgraphics" ALIAS "ofx_gfx_shape_ray2_at" (ray() AS DOUBLE, t AS DOUBLE, out() AS DOUBLE) AS LONG
