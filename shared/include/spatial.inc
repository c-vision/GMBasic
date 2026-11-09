' spatial.inc — namespace SPATIAL via FFI (lib `spatial`, libgmbspatial).
'
' scipy.spatial.distance (PDIST, CDIST, DISTANCEMATRIX) e ConvexHull.
' La lib e' self-contained (solo libm); i punti arrivano come matrici flat
' row-major. Il wrapper appiattisce gli array jagged del linguaggio
' (DIM X(n) AS ARRAY OF DOUBLE) e, per CDIST/DISTANCEMATRIX, ricostruisce la
' MATRIX (handle LONG) con MATRIX.CREATE/MATRIX.SETVALUE: la superficie
' restituisce quindi lo stesso handle di prima (MATRIX.GETVALUE continua a
' funzionare).
'
' Uso: IMPORT SPATIAL   (shared/libs: nessun REQUIRE; export in shared/include)

NAMESPACE SPATIAL

DECLARE NATIVE FUNCTION SPN_PDIST LIB "gmbspatial" ALIAS "ofx_sp_pdist" (X() AS DOUBLE, n AS LONG, d AS LONG, metric AS STRING, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SPN_CDIST LIB "gmbspatial" ALIAS "ofx_sp_cdist" (X() AS DOUBLE, nx AS LONG, dx AS LONG, Y() AS DOUBLE, ny AS LONG, dy AS LONG, metric AS STRING, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SPN_DISTMAT LIB "gmbspatial" ALIAS "ofx_sp_distance_matrix" (X() AS DOUBLE, nx AS LONG, dx AS LONG, Y() AS DOUBLE, ny AS LONG, dy AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SPN_HULL LIB "gmbspatial" ALIAS "ofx_sp_convex_hull" (X() AS DOUBLE, n AS LONG, stride AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG

' PDIST(X, metric) — distanze a coppie condensate (triangolo superiore, i<j).
' X e' jagged (n righe x d colonne). Metric: euclidean|cityblock|chebyshev|cosine.
PUBLIC FUNCTION PDIST(X() AS ARRAY OF DOUBLE, metric AS STRING) AS ARRAY OF DOUBLE
    DIM n AS LONG, d AS LONG, m AS LONG
    n = UBOUND(X) + 1
    d = UBOUND(X(0)) + 1
    m = n * (n - 1) / 2
    IF m < 1 THEN
        DIM z(0) AS DOUBLE
        z(0) = 0
        RETURN z
    END IF
    DIM flat(n * d - 1) AS DOUBLE
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO n - 1
        FOR j = 0 TO d - 1
            flat(i * d + j) = X(i)(j)
        NEXT j
    NEXT i
    DIM o(m - 1) AS DOUBLE
    CALL SPN_PDIST(flat, n, d, metric, o, m)
    RETURN o
END FUNCTION

' CDIST(X, Y, metric) — matrice nx*ny delle distanze tra ogni riga di X e Y.
' Ritorna l'handle MATRIX (LONG).
PUBLIC FUNCTION CDIST(X() AS ARRAY OF DOUBLE, Y() AS ARRAY OF DOUBLE, metric AS STRING) AS LONG
    DIM nx AS LONG, ny AS LONG, dx AS LONG, dy AS LONG
    nx = UBOUND(X) + 1
    ny = UBOUND(Y) + 1
    dx = UBOUND(X(0)) + 1
    dy = UBOUND(Y(0)) + 1
    DIM fx(nx * dx - 1) AS DOUBLE
    DIM fy(ny * dy - 1) AS DOUBLE
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO nx - 1
        FOR j = 0 TO dx - 1
            fx(i * dx + j) = X(i)(j)
        NEXT j
    NEXT i
    FOR i = 0 TO ny - 1
        FOR j = 0 TO dy - 1
            fy(i * dy + j) = Y(i)(j)
        NEXT j
    NEXT i
    DIM o(nx * ny - 1) AS DOUBLE
    CALL SPN_CDIST(fx, nx, dx, fy, ny, dy, metric, o, nx * ny)
    DIM m AS LONG
    m = MATRIX.CREATE(nx, ny)
    FOR i = 0 TO nx - 1
        FOR j = 0 TO ny - 1
            m = MATRIX.SETVALUE(m, i, j, o(i * ny + j))
        NEXT j
    NEXT i
    RETURN m
END FUNCTION

' DISTANCEMATRIX(X, Y) — come CDIST con metrica euclidea.
PUBLIC FUNCTION DISTANCEMATRIX(X() AS ARRAY OF DOUBLE, Y() AS ARRAY OF DOUBLE) AS LONG
    RETURN CDIST(X, Y, "euclidean")
END FUNCTION

' CONVEXHULL(X) — indici dei vertici dell'inviluppo convesso (punti 2D).
PUBLIC FUNCTION CONVEXHULL(X() AS ARRAY OF DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG, k AS LONG
    n = UBOUND(X) + 1
    IF n < 3 THEN
        DIM z(0) AS DOUBLE
        z(0) = 0
        RETURN z
    END IF
    DIM flat(n * 2 - 1) AS DOUBLE
    DIM i AS LONG
    FOR i = 0 TO n - 1
        flat(i * 2) = X(i)(0)
        flat(i * 2 + 1) = X(i)(1)
    NEXT i
    DIM o(n - 1) AS DOUBLE
    k = SPN_HULL(flat, n, 2, o, n)
    IF k < 1 THEN
        REDIM PRESERVE o(0)
        o(0) = 0
        RETURN o
    END IF
    REDIM PRESERVE o(k - 1)
    RETURN o
END FUNCTION
