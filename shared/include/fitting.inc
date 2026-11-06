' fitting.inc — namespace FITTING via FFI (libgmbstats: gmbsuite/gmbstats-fitting).
' Primitive native con dimensioni esplicite + wrapper PUBLIC che le deducono dagli
' array (UBOUND+1). Output: coef(m+1) per POLYFIT, out(2) per RSQUARED (r2, angle).
NAMESPACE FITTING

DECLARE FUNCTION FXN_POLYFIT LIB "gmbstats" ALIAS "ofx_fit_polyfit" (x() AS DOUBLE, y() AS DOUBLE, n AS LONG, m AS LONG, coef() AS DOUBLE) AS DOUBLE
DECLARE FUNCTION FXN_POLYEVAL LIB "gmbstats" ALIAS "gs_fit_poly_eval" (x AS DOUBLE, coef() AS DOUBLE, m AS LONG) AS DOUBLE
DECLARE FUNCTION FXN_CHEBEVAL LIB "gmbstats" ALIAS "gs_fit_cheb_eval" (x AS DOUBLE, coef() AS DOUBLE, m AS LONG) AS DOUBLE
DECLARE FUNCTION FXN_PADEEVAL LIB "gmbstats" ALIAS "gs_fit_pade_eval" (x AS DOUBLE, a() AS DOUBLE, na AS LONG, b() AS DOUBLE, nb AS LONG) AS DOUBLE
DECLARE SUB FXN_RSQUARED LIB "gmbstats" ALIAS "ofx_fit_rsquared" (data() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE FUNCTION FXN_RSQUAREDTEST LIB "gmbstats" ALIAS "gs_rsquared_test" (data() AS DOUBLE, n AS LONG) AS DOUBLE

' coef() deve avere m+1 elementi; m = grado del polinomio (esplicito).
PUBLIC FUNCTION POLYFIT(x() AS DOUBLE, y() AS DOUBLE, coef() AS DOUBLE) AS DOUBLE
    ' Il grado m e' dedotto da coef(): coef ha m+1 elementi.
    RETURN FXN_POLYFIT(x, y, UBOUND(x) + 1, UBOUND(coef), coef)
END FUNCTION

' gs_fit_poly_eval vuole l'INDICE massimo del coefficiente (= UBOUND).
PUBLIC FUNCTION POLYEVAL(x AS DOUBLE, coef() AS DOUBLE) AS DOUBLE
    RETURN FXN_POLYEVAL(x, coef, UBOUND(coef))
END FUNCTION

' gs_fit_cheb_eval vuole il NUMERO di coefficienti.
PUBLIC FUNCTION CHEBEVAL(x AS DOUBLE, coef() AS DOUBLE) AS DOUBLE
    RETURN FXN_CHEBEVAL(x, coef, UBOUND(coef) + 1)
END FUNCTION

' na = numero di coefficienti a; nb = indice massimo di b.
PUBLIC FUNCTION PADEEVAL(x AS DOUBLE, a() AS DOUBLE, b() AS DOUBLE) AS DOUBLE
    RETURN FXN_PADEEVAL(x, a, UBOUND(a) + 1, b, UBOUND(b))
END FUNCTION

PUBLIC SUB RSQUARED(data() AS DOUBLE, out() AS DOUBLE)
    CALL FXN_RSQUARED(data, UBOUND(data) + 1, out)
END SUB

PUBLIC FUNCTION RSQUAREDTEST(data() AS DOUBLE) AS DOUBLE
    RETURN FXN_RSQUAREDTEST(data, UBOUND(data) + 1)
END FUNCTION
