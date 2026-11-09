' spatial.inc — namespace SPATIAL via FFI (lib `spatial`, libgmbspatial).
'
' scipy.spatial.distance (PDIST, CDIST, DISTANCEMATRIX) e ConvexHull.
' I punti sono MATRICI rank-2 (`MATRIX OF (INT, INT, DOUBLE)`): il descrittore
' viene passato all'FFI come buffer flat row-major, senza flattening manuale.
'
' Uso: IMPORT SPATIAL   (shared/libs: nessun REQUIRE; export in shared/include)

NAMESPACE SPATIAL

DECLARE NATIVE FUNCTION SPN_PDIST LIB "gmbspatial" ALIAS "ofx_sp_pdist" (X AS MATRIX OF (INT, INT, DOUBLE), n AS LONG, d AS LONG, metric AS STRING, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SPN_CDIST LIB "gmbspatial" ALIAS "ofx_sp_cdist" (X AS MATRIX OF (INT, INT, DOUBLE), nx AS LONG, dx AS LONG, Y AS MATRIX OF (INT, INT, DOUBLE), ny AS LONG, dy AS LONG, metric AS STRING, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SPN_DISTMAT LIB "gmbspatial" ALIAS "ofx_sp_distance_matrix" (X AS MATRIX OF (INT, INT, DOUBLE), nx AS LONG, dx AS LONG, Y AS MATRIX OF (INT, INT, DOUBLE), ny AS LONG, dy AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SPN_HULL LIB "gmbspatial" ALIAS "ofx_sp_convex_hull" (X AS MATRIX OF (INT, INT, DOUBLE), n AS LONG, stride AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG

' PDIST(X, metric) — distanze a coppie condensate (triangolo superiore, i<j).
PUBLIC FUNCTION PDIST(X AS MATRIX OF (INT, INT, DOUBLE), metric AS STRING) AS ARRAY OF DOUBLE
    DIM n AS LONG, d AS LONG, m AS LONG
    n = UBOUND(X, 1) + 1
    d = UBOUND(X, 2) + 1
    m = n * (n - 1) / 2
    IF m < 1 THEN
        DIM z(0) AS DOUBLE
        z(0) = 0
        RETURN z
    END IF
    DIM o(m - 1) AS DOUBLE
    CALL SPN_PDIST(X, n, d, metric, o, m)
    RETURN o
END FUNCTION

' CDIST(X, Y, metric) — matrice nx*ny delle distanze tra ogni riga di X e Y.
' Ritorna l'handle MATRIX (LONG) — namespace MATRIX (finché non migrato a valore).
PUBLIC FUNCTION CDIST(X AS MATRIX OF (INT, INT, DOUBLE), Y AS MATRIX OF (INT, INT, DOUBLE), metric AS STRING) AS LONG
    DIM nx AS LONG, ny AS LONG, dx AS LONG, dy AS LONG
    nx = UBOUND(X, 1) + 1
    dx = UBOUND(X, 2) + 1
    ny = UBOUND(Y, 1) + 1
    dy = UBOUND(Y, 2) + 1
    IF nx < 1 OR ny < 1 THEN
        RETURN 0
    END IF
    DIM o(nx * ny - 1) AS DOUBLE
    CALL SPN_CDIST(X, nx, dx, Y, ny, dy, metric, o, nx * ny)
    DIM m AS LONG
    m = MATRIX.CREATE(nx, ny)
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO nx - 1
        FOR j = 0 TO ny - 1
            m = MATRIX.SETVALUE(m, i, j, o(i * ny + j))
        NEXT j
    NEXT i
    RETURN m
END FUNCTION

' DISTANCEMATRIX(X, Y) — come CDIST con metrica euclidea.
PUBLIC FUNCTION DISTANCEMATRIX(X AS MATRIX OF (INT, INT, DOUBLE), Y AS MATRIX OF (INT, INT, DOUBLE)) AS LONG
    RETURN CDIST(X, Y, "euclidean")
END FUNCTION

' CONVEXHULL(X) — indici dei vertici dell'inviluppo convesso (punti 2D).
PUBLIC FUNCTION CONVEXHULL(X AS MATRIX OF (INT, INT, DOUBLE)) AS ARRAY OF DOUBLE
    DIM n AS LONG, d AS LONG, k AS LONG
    n = UBOUND(X, 1) + 1
    IF n < 3 THEN
        DIM z(0) AS DOUBLE
        z(0) = 0
        RETURN z
    END IF
    d = UBOUND(X, 2) + 1
    DIM o(n - 1) AS DOUBLE
    k = SPN_HULL(X, n, d, o, n)
    IF k < 1 THEN
        REDIM PRESERVE o(0)
        o(0) = 0
        RETURN o
    END IF
    REDIM PRESERVE o(k - 1)
    RETURN o
END FUNCTION
