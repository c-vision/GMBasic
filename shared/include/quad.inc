' quad.inc — namespace QUAD: quadratura numerica (gmbmath-numerical, FFI).
'
' NUMERICAL.INTEGRATE è Simpson a numero di passi FISSO. Qui ci sono i metodi che
' mancano, dove il numero di valutazioni lo decide l'algoritmo:
'
'   KRONROD    Gauss-Kronrod G7-K15, con STIMA D'ERRORE (out_(1))
'   GAUSS      Gauss-Legendre a n nodi (esatto per polinomi fino a grado 2n-1)
'   ROMBERG    estrapolazione di Richardson su trapezi, fino a tolleranza
'   TANHSINH   double-exponential (Takahasi-Mori): robusta con singolarità agli estremi
'   TANHSINHCOMP  variante che passa anche d = distanza dall'estremo più vicino
'              (per integrande come 1/sqrt(x): scriverle in funzione di d)
'
' LEGENDRENODES e HERMITENODES danno nodi e pesi, per costruire quadrature proprie
' (per esempio su domini scomposti o con pesi non standard).
'
' La callback è una FUNCTION del programma: `(x AS DOUBLE) AS DOUBLE`, oppure
' `(x AS DOUBLE, d AS DOUBLE) AS DOUBLE` per TANHSINHCOMP. Viene invocata in modo
' sincrono; non deve a sua volta chiamare funzioni di quadratura (il ponte usa uno
' stato globale, come in OPTIMIZE.SHAPLEY).
'
' Ritorno: 0 = ok, -1 = argomento non valido; le funzioni che ritornano DOUBLE danno
' NaN se la callback è nulla.

NAMESPACE QUAD

' Gauss-Kronrod G7-K15: out_(0) = integrale, out_(1) = errore stimato.
DECLARE FUNCTION KRONROD LIB "gmbmath" ALIAS "ofx_quad_kronrod" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, out_() AS DOUBLE) AS LONG
' Gauss-Legendre a n nodi (n >= 1).
DECLARE FUNCTION GAUSS LIB "gmbmath" ALIAS "ofx_quad_gauss" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, n AS LONG) AS DOUBLE
' Romberg con tolleranza tol e maxit livelli.
DECLARE FUNCTION ROMBERG LIB "gmbmath" ALIAS "ofx_quad_romberg" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, tol AS DOUBLE, maxit AS LONG) AS DOUBLE
' tanh-sinh: absacc = accuratezza assoluta, maxeval = limite di valutazioni.
DECLARE FUNCTION TANHSINH LIB "gmbmath" ALIAS "ofx_quad_tanh_sinh" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, absacc AS DOUBLE, maxeval AS LONG) AS DOUBLE
' tanh-sinh con distanza dall'estremo: f(x, d) dove d = distanza firmata dall'estremo più vicino.
DECLARE FUNCTION TANHSINHCOMP LIB "gmbmath" ALIAS "ofx_quad_tanh_sinh_c" (f AS FUNCTION(x AS DOUBLE, d AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, absacc AS DOUBLE, maxeval AS LONG) AS DOUBLE
' Nodi e pesi di Gauss-Legendre su [-1,1]; x() e w() di n elementi. Ritorna n.
DECLARE FUNCTION LEGENDRENODES LIB "gmbmath" ALIAS "ofx_quad_legendre_nodes" (n AS LONG, x() AS DOUBLE, w() AS DOUBLE) AS LONG
' Nodi e pesi di Gauss-Hermite (integrale su R con peso e^{-x^2}). Ritorna n.
DECLARE FUNCTION HERMITENODES LIB "gmbmath" ALIAS "ofx_quad_hermite_nodes" (n AS LONG, x() AS DOUBLE, w() AS DOUBLE) AS LONG
