' fitting.inc — namespace FITTING via FFI (libgmbstats: gmbsuite/gmbstats-fitting).
' Output array: coef(m+1) per POLYFIT, out(2) per RSQUARED (r2, angle).
NAMESPACE FITTING

PUBLIC DECLARE FUNCTION POLYFIT LIB "gmbstats" ALIAS "ofx_fit_polyfit" (x() AS DOUBLE, y() AS DOUBLE, n AS LONG, m AS LONG, coef() AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION POLYEVAL LIB "gmbstats" ALIAS "gs_fit_poly_eval" (x AS DOUBLE, coef() AS DOUBLE, m AS LONG) AS DOUBLE
PUBLIC DECLARE FUNCTION CHEBEVAL LIB "gmbstats" ALIAS "gs_fit_cheb_eval" (x AS DOUBLE, coef() AS DOUBLE, m AS LONG) AS DOUBLE
PUBLIC DECLARE FUNCTION PADEEVAL LIB "gmbstats" ALIAS "gs_fit_pade_eval" (x AS DOUBLE, a() AS DOUBLE, na AS LONG, b() AS DOUBLE, nb AS LONG) AS DOUBLE
PUBLIC DECLARE SUB RSQUARED LIB "gmbstats" ALIAS "ofx_fit_rsquared" (data() AS DOUBLE, n AS LONG, out() AS DOUBLE)
PUBLIC DECLARE FUNCTION RSQUAREDTEST LIB "gmbstats" ALIAS "gs_rsquared_test" (data() AS DOUBLE, n AS LONG) AS DOUBLE
