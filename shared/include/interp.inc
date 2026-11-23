' interp.inc — namespace INTERP via FFI (lib `math`): spline cubica not-a-knot
' (scipy.interpolate.splrep/splev).
'
' Uso: IMPORT INTERP
'
'   SPLREP(x(), y())            → array combinato [n, x0…x(n-1), coeff(4(n-1))]
'   SPLEV(spline(), xq())       → valori della spline nei punti xq
'
' ABI del pacchetto: buffer del chiamante; le PUBLIC allocano la dimensione esatta
' (5n-3 per SPLREP, LEN(xq) per SPLEV). La matematica vive in gmi_interp.c del
' pacchetto `math` (seconda derivata/2 con Thomas O(n), non il sistema denso n×n).

NAMESPACE INTERP

DECLARE NATIVE FUNCTION SXF_SPLREP LIB "gmbmath" ALIAS "ofx_interp_splrep" (x() AS DOUBLE, y() AS DOUBLE, n AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG
DECLARE NATIVE FUNCTION SXF_SPLEV LIB "gmbmath" ALIAS "ofx_interp_splev" (spline() AS DOUBLE, slen AS LONG, xq() AS DOUBLE, nq AS LONG, out() AS DOUBLE, cap AS LONG) AS LONG

PUBLIC FUNCTION SPLREP(x() AS DOUBLE, y() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(x) + 1
    DIM len AS LONG = 5 * n - 3
    DIM o(len - 1) AS DOUBLE
    CALL SXF_SPLREP(x, y, n, o, len)
    RETURN o
END FUNCTION

PUBLIC FUNCTION SPLEV(spline() AS DOUBLE, xq() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM slen AS LONG = UBOUND(spline) + 1
    DIM nq AS LONG = UBOUND(xq) + 1
    DIM o(nq - 1) AS DOUBLE
    CALL SXF_SPLEV(spline, slen, xq, nq, o, nq)
    RETURN o
END FUNCTION
