' veccalc.inc — namespace VECCALC: calcolo vettoriale (gmbmath-numerical, FFI).
'
' Gradiente, divergenza, rotore, laplaciano, triedro di Frenet, lunghezza d'arco,
' integrali di linea e flusso attraverso una superficie — tutti per differenze
' centrali sul campo dato dall'utente.
'
' Forme delle callback (BASIC):
'   campo scalare    f(x, y, z)                     AS DOUBLE
'   campo vettoriale F(x, y, z)                     AS ARRAY OF DOUBLE (3 elementi)
'   curva            r(t)                           AS ARRAY OF DOUBLE (3 elementi)
'   patch            S(u, v)                        AS ARRAY OF DOUBLE (3 elementi)
'
' Gli array di uscita sono dichiarati dal chiamante. Ritorno: 0 = ok, -1 = argomenti
' non validi (dove c'è un array di uscita); le funzioni che ritornano DOUBLE danno 0
' se la callback è nulla o il passo è 0.

NAMESPACE VECCALC

' ── campo scalare ───────────────────────────────────────────────────────────
' Gradiente per differenze centrali di passo h: out_() riceve le 3 derivate parziali.
DECLARE FUNCTION GRADIENT LIB "gmbmath" ALIAS "ofx_vc_gradient" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS DOUBLE, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, h AS DOUBLE, out_() AS DOUBLE) AS LONG
' Laplaciano (stencil a 7 punti di passo h).
DECLARE FUNCTION LAPLACIAN LIB "gmbmath" ALIAS "ofx_vc_laplacian" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS DOUBLE, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, h AS DOUBLE) AS DOUBLE

' ── campo vettoriale ────────────────────────────────────────────────────────
' Divergenza per differenze centrali.
DECLARE FUNCTION DIVERGENCE LIB "gmbmath" ALIAS "ofx_vc_divergence" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, h AS DOUBLE) AS DOUBLE
' Rotore: out_() riceve le 3 componenti.
DECLARE FUNCTION CURL LIB "gmbmath" ALIAS "ofx_vc_curl" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, h AS DOUBLE, out_() AS DOUBLE) AS LONG
' 1 se |div F| <= tol (campo solenoidale).
DECLARE FUNCTION ISSOLENOIDAL LIB "gmbmath" ALIAS "ofx_vc_is_solenoidal" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, h AS DOUBLE, tol AS DOUBLE) AS LONG
' 1 se |curl F| <= tol (campo irrotazionale).
DECLARE FUNCTION ISIRROTATIONAL LIB "gmbmath" ALIAS "ofx_vc_is_irrotational" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY, x AS DOUBLE, y AS DOUBLE, z AS DOUBLE, h AS DOUBLE, tol AS DOUBLE) AS LONG

' ── curva ───────────────────────────────────────────────────────────────────
' Lunghezza d'arco di r(t) su [a,b] (Simpson adattiva, tol).
DECLARE FUNCTION ARCLENGTH LIB "gmbmath" ALIAS "ofx_vc_arc_length" (r AS FUNCTION(t AS DOUBLE) AS ARRAY, a AS DOUBLE, b AS DOUBLE, tol AS DOUBLE) AS DOUBLE
' Triedro di Frenet in t: out_(0..2)=T, out_(3..5)=N, out_(6..8)=B, out_(9)=kappa,
' out_(10)=tau.
DECLARE FUNCTION TNB LIB "gmbmath" ALIAS "ofx_vc_tnb" (r AS FUNCTION(t AS DOUBLE) AS ARRAY, t AS DOUBLE, h AS DOUBLE, out_() AS DOUBLE) AS LONG

' ── integrali di linea e flusso ─────────────────────────────────────────────
' Integrale di linea di un campo scalare: f(r(t))·|r''(t)| su [a,b].
DECLARE FUNCTION LINESCALAR LIB "gmbmath" ALIAS "ofx_vc_line_scalar" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS DOUBLE, r AS FUNCTION(t AS DOUBLE) AS ARRAY, a AS DOUBLE, b AS DOUBLE, tol AS DOUBLE) AS DOUBLE
' Lavoro: integrale di linea di un campo vettoriale F(r(t))·r''(t) su [a,b].
DECLARE FUNCTION LINEWORK LIB "gmbmath" ALIAS "ofx_vc_line_work" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY, r AS FUNCTION(t AS DOUBLE) AS ARRAY, a AS DOUBLE, b AS DOUBLE, tol AS DOUBLE) AS DOUBLE
' Flusso di F attraverso la patch S(u,v) su [u0,u1]x[v0,v1].
DECLARE FUNCTION FLUX LIB "gmbmath" ALIAS "ofx_vc_flux" (f AS FUNCTION(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY, s AS FUNCTION(u AS DOUBLE, v AS DOUBLE) AS ARRAY, u0 AS DOUBLE, u1 AS DOUBLE, v0 AS DOUBLE, v1 AS DOUBLE, tol AS DOUBLE) AS DOUBLE
