' kdtree.inc — namespace KDTREE via FFI (lib `kdtree`, libgmbkdtree).
'
' scipy.spatial.KDTree: CREATE su punti flat (n x m), query k-NN (indici o
' distanze) e query_ball_point. La lib e' self-contained (tabella statica di
' handle int).
'
' Uso: IMPORT KDTREE   (shared/libs: nessun REQUIRE; export in shared/include)
'   DIM h = KDTREE.CREATE(punti, m)     ' punti() e' flat row-major
'   DIM idx()  = KDTREE.QUERY(h, x, k)
'   DIM dist() = KDTREE.QUERYDIST(h, x, k)
'   DIM ball() = KDTREE.QUERYBALL(h, x, r)

NAMESPACE KDTREE

DECLARE NATIVE FUNCTION KDN_CREATE LIB "gmbkdtree" ALIAS "ofx_kd_create" (points() AS DOUBLE, n AS LONG, m AS LONG) AS LONG
DECLARE NATIVE FUNCTION KDN_COUNT LIB "gmbkdtree" ALIAS "ofx_kd_count" (h AS LONG) AS LONG
DECLARE NATIVE FUNCTION KDN_QUERY LIB "gmbkdtree" ALIAS "ofx_kd_query" (h AS LONG, x() AS DOUBLE, k AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION KDN_QUERYDIST LIB "gmbkdtree" ALIAS "ofx_kd_query_dist" (h AS LONG, x() AS DOUBLE, k AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION KDN_QUERYBALL LIB "gmbkdtree" ALIAS "ofx_kd_query_ball" (h AS LONG, x() AS DOUBLE, r AS DOUBLE, out() AS DOUBLE, cap AS LONG) AS LONG

' CREATE(points, m) — costruisce l'albero da n = LEN(points)/m punti di m dimensioni.
PUBLIC FUNCTION CREATE(points() AS DOUBLE, m AS LONG) AS LONG
    DIM n AS LONG
    n = (UBOUND(points) + 1) / m
    RETURN KDN_CREATE(points, n, m)
END FUNCTION

' QUERY(h, x, k) — indici dei k punti più vicini a x (ordinati per distanza).
PUBLIC FUNCTION QUERY(h AS LONG, x() AS DOUBLE, k AS LONG) AS ARRAY OF DOUBLE
    IF k < 1 THEN
        DIM z(0) AS DOUBLE
        z(0) = 0
        RETURN z
    END IF
    DIM o(k - 1) AS DOUBLE
    DIM c AS LONG
    c = KDN_QUERY(h, x, k, o, k)
    IF c < 1 THEN
        REDIM PRESERVE o(0)
        o(0) = 0
        RETURN o
    END IF
    IF c < k THEN REDIM PRESERVE o(c - 1)
    RETURN o
END FUNCTION

' QUERYDIST(h, x, k) — distanze euclidee dei k punti più vicini a x.
PUBLIC FUNCTION QUERYDIST(h AS LONG, x() AS DOUBLE, k AS LONG) AS ARRAY OF DOUBLE
    IF k < 1 THEN
        DIM z(0) AS DOUBLE
        z(0) = 0
        RETURN z
    END IF
    DIM o(k - 1) AS DOUBLE
    DIM c AS LONG
    c = KDN_QUERYDIST(h, x, k, o, k)
    IF c < 1 THEN
        REDIM PRESERVE o(0)
        o(0) = 0
        RETURN o
    END IF
    IF c < k THEN REDIM PRESERVE o(c - 1)
    RETURN o
END FUNCTION

' QUERYBALL(h, x, r) — indici di tutti i punti entro il raggio r da x.
PUBLIC FUNCTION QUERYBALL(h AS LONG, x() AS DOUBLE, r AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG
    n = KDN_COUNT(h)
    IF n < 1 THEN
        DIM z(0) AS DOUBLE
        z(0) = 0
        RETURN z
    END IF
    DIM o(n - 1) AS DOUBLE
    DIM c AS LONG
    c = KDN_QUERYBALL(h, x, r, o, n)
    IF c < 1 THEN
        REDIM PRESERVE o(0)
        o(0) = 0
        RETURN o
    END IF
    IF c < n THEN REDIM PRESERVE o(c - 1)
    RETURN o
END FUNCTION
