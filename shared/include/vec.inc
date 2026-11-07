' vec.inc — namespace VEC: operazioni vettoriali che VECTOR non ha (gmbmath-vector, FFI).
'
' VECTOR copre già somma, differenza, prodotto scalare e vettoriale, norma, distanze,
' lerp e min/max per componente: qui NON vengono ripetute.
'
' Questo namespace aggiunge:
'   PROJECT / REJECT   componente parallela e perpendicolare a un altro vettore
'   SCHLICK            Fresnel approssimato (riflessione in funzione dell'angolo)
'   BIAS               scostamento di un punto lungo una normale (ray tracing)
'   VCLAMP/VCLAMP01/VCLAMPMAG  limiti per componente e di modulo
'   VABS / VSQRT / VPOW        valore assoluto, radice e potenza per componente
'   PERP2D             perpendicolare di un vettore 2D
'
' I prefissi: `V` sui nomi che sono anche funzioni scalari del linguaggio (ABS, SQRT,
' POW, CLAMP), per non essere risolti sul builtin scalare; gli altri restano naturali.
'
' I vettori sono array di 3 double (2 per PERP2D); out_() è dichiarato dal chiamante.
' Ritorno: 0 = ok, -1 = array troppo corto.

NAMESPACE VEC

' ── scomposizione rispetto a un altro vettore ───────────────────────────────
' Proiezione di v() su on_(): on * (v·on / |on|^2); on nullo -> vettore zero.
DECLARE FUNCTION PROJECT LIB "gmbmath" ALIAS "ofx_vec_project" (v() AS DOUBLE, on_() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Componente perpendicolare a on_(): v - PROJECT(v, on). Utile per scivolare su un piano.
DECLARE FUNCTION REJECT LIB "gmbmath" ALIAS "ofx_vec_reject" (v() AS DOUBLE, on_() AS DOUBLE, out_() AS DOUBLE) AS LONG

' ── grafica ─────────────────────────────────────────────────────────────────
' Fresnel di Schlick per componente: f0*(1-f) + f, con f = (1-u)^5.
' f0 = riflettanza a incidenza normale, u = coseno dell'angolo di incidenza.
DECLARE FUNCTION SCHLICK LIB "gmbmath" ALIAS "ofx_vec_schlick" (f0() AS DOUBLE, u AS DOUBLE, out_() AS DOUBLE) AS LONG
' Bias: v + normale*eps (eps proporzionale alla magnitudine). Evita l'auto-intersezione.
DECLARE FUNCTION BIAS LIB "gmbmath" ALIAS "ofx_vec_bias" (v() AS DOUBLE, nrm() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Perpendicolare 2D: (-y, x). out_() riceve 2 double.
DECLARE FUNCTION PERP2D LIB "gmbmath" ALIAS "ofx_vec_perp2d" (x AS DOUBLE, y AS DOUBLE, out_() AS DOUBLE) AS LONG

' ── limiti e funzioni per componente ────────────────────────────────────────
' Clamp di ogni componente in [lo, hi].
DECLARE FUNCTION VCLAMP LIB "gmbmath" ALIAS "ofx_vec_clamp" (v() AS DOUBLE, lo AS DOUBLE, hi AS DOUBLE, out_() AS DOUBLE) AS LONG
' Clamp di ogni componente in [0, 1].
DECLARE FUNCTION VCLAMP01 LIB "gmbmath" ALIAS "ofx_vec_clamp01" (v() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Limita il modulo a maxlen lasciando la direzione.
DECLARE FUNCTION VCLAMPMAG LIB "gmbmath" ALIAS "ofx_vec_clamp_mag" (v() AS DOUBLE, maxlen AS DOUBLE, out_() AS DOUBLE) AS LONG
' Valore assoluto per componente.
DECLARE FUNCTION VABS LIB "gmbmath" ALIAS "ofx_vec_abs" (v() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Radice quadrata per componente (negative -> NaN, come in C).
DECLARE FUNCTION VSQRT LIB "gmbmath" ALIAS "ofx_vec_sqrt" (v() AS DOUBLE, out_() AS DOUBLE) AS LONG
' Potenza per componente.
DECLARE FUNCTION VPOW LIB "gmbmath" ALIAS "ofx_vec_pow" (v() AS DOUBLE, p AS DOUBLE, out_() AS DOUBLE) AS LONG
