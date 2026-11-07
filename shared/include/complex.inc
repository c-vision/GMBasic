' complex.inc — namespace COMPLEX: numeri complessi (gmbmath-core, FFI).
'
' Un numero complesso è il TYPE `Complex` (parte reale RE, parte immaginaria IM), con lo
' stesso layout della struct C `gm_cpx` (2 double). Viaggia **per valore**, come GeoPoint
' di GEO: `z = COMPLEX.MAKE(1, 2)`, nessun handle e nessuno stato.
'
' Casi limite della libreria C (documentati, non nascosti):
'   DIV per divisore nullo → (0,0)
'   POW con base (0,0)     → (0,0)
'   SQRT di (0,0)          → (0,0)
'   LOG di (0,0)           → (-infinito, 0)
' Usare EQ con una tolleranza per confrontare: i trascendenti complessi non sono esatti.
'
' Perché alcune funzioni hanno il prefisso `C` (CABS, CEXP, CSIN, …): sono nomi che
' esistono già come funzioni scalari del linguaggio (ABS, EXP, SIN, …) e su IL/PCode/VM-C
' la forma qualificata `COMPLEX.ABS(z)` verrebbe risolta come builtin scalare invece che
' come funzione dichiarata in questo file. `CABS`/`CEXP`/… è anche la convenzione di
' <complex.h> del C99, quindi il nome resta riconoscibile.

NAMESPACE COMPLEX

TYPE Complex
  RE AS DOUBLE
  IM AS DOUBLE
END TYPE

' ── costruzione e aritmetica ────────────────────────────────────────────────
DECLARE FUNCTION MAKE LIB "gmbmath" ALIAS "ofx_cpx_make" (re AS DOUBLE, im AS DOUBLE) AS Complex
DECLARE FUNCTION ADD LIB "gmbmath" ALIAS "ofx_cpx_add" (a AS Complex, b AS Complex) AS Complex
DECLARE FUNCTION SUB LIB "gmbmath" ALIAS "ofx_cpx_sub" (a AS Complex, b AS Complex) AS Complex
DECLARE FUNCTION MUL LIB "gmbmath" ALIAS "ofx_cpx_mul" (a AS Complex, b AS Complex) AS Complex
DECLARE FUNCTION DIV LIB "gmbmath" ALIAS "ofx_cpx_div" (a AS Complex, b AS Complex) AS Complex
DECLARE FUNCTION NEG LIB "gmbmath" ALIAS "ofx_cpx_neg" (a AS Complex) AS Complex
' Coniugato: (re, -im).
DECLARE FUNCTION CONJ LIB "gmbmath" ALIAS "ofx_cpx_conj" (a AS Complex) AS Complex
' Moltiplicazione per uno scalare reale.
DECLARE FUNCTION SCALE LIB "gmbmath" ALIAS "ofx_cpx_scale" (a AS Complex, s AS DOUBLE) AS Complex

' ── misure ──────────────────────────────────────────────────────────────────
' Modulo |z|.
DECLARE FUNCTION CABS LIB "gmbmath" ALIAS "ofx_cpx_abs" (a AS Complex) AS DOUBLE
' Argomento (fase) in radianti, in (-pi, pi].
DECLARE FUNCTION ARG LIB "gmbmath" ALIAS "ofx_cpx_arg" (a AS Complex) AS DOUBLE
' Modulo al quadrato re^2+im^2 (evita la radice).
DECLARE FUNCTION NORM2 LIB "gmbmath" ALIAS "ofx_cpx_norm2" (a AS Complex) AS DOUBLE
' Uguaglianza con tolleranza relativa 1e-9 (scalata sui moduli): 1 = uguali.
' Per una tolleranza propria: COMPLEX.CABS(COMPLEX.SUB(a, b)) < tol.
DECLARE FUNCTION EQ LIB "gmbmath" ALIAS "ofx_cpx_eq" (a AS Complex, b AS Complex) AS LONG

' ── esponenziale, logaritmo, potenza ────────────────────────────────────────
DECLARE FUNCTION CEXP LIB "gmbmath" ALIAS "ofx_cpx_exp" (z AS Complex) AS Complex
DECLARE FUNCTION CLOG LIB "gmbmath" ALIAS "ofx_cpx_log" (z AS Complex) AS Complex
DECLARE FUNCTION CSQRT LIB "gmbmath" ALIAS "ofx_cpx_sqrt" (z AS Complex) AS Complex
' z^w per esponenziazione complessa (w può essere complesso).
DECLARE FUNCTION POW LIB "gmbmath" ALIAS "ofx_cpx_pow" (z AS Complex, w AS Complex) AS Complex

' ── trigonometriche ─────────────────────────────────────────────────────────
DECLARE FUNCTION CSIN LIB "gmbmath" ALIAS "ofx_cpx_sin" (z AS Complex) AS Complex
DECLARE FUNCTION CCOS LIB "gmbmath" ALIAS "ofx_cpx_cos" (z AS Complex) AS Complex
DECLARE FUNCTION CTAN LIB "gmbmath" ALIAS "ofx_cpx_tan" (z AS Complex) AS Complex
DECLARE FUNCTION CASIN LIB "gmbmath" ALIAS "ofx_cpx_asin" (z AS Complex) AS Complex
DECLARE FUNCTION CACOS LIB "gmbmath" ALIAS "ofx_cpx_acos" (z AS Complex) AS Complex
DECLARE FUNCTION CATAN LIB "gmbmath" ALIAS "ofx_cpx_atan" (z AS Complex) AS Complex

' ── iperboliche ─────────────────────────────────────────────────────────────
DECLARE FUNCTION CSINH LIB "gmbmath" ALIAS "ofx_cpx_sinh" (z AS Complex) AS Complex
DECLARE FUNCTION CCOSH LIB "gmbmath" ALIAS "ofx_cpx_cosh" (z AS Complex) AS Complex
DECLARE FUNCTION CTANH LIB "gmbmath" ALIAS "ofx_cpx_tanh" (z AS Complex) AS Complex
DECLARE FUNCTION ASINH LIB "gmbmath" ALIAS "ofx_cpx_asinh" (z AS Complex) AS Complex
DECLARE FUNCTION ACOSH LIB "gmbmath" ALIAS "ofx_cpx_acosh" (z AS Complex) AS Complex
DECLARE FUNCTION ATANH LIB "gmbmath" ALIAS "ofx_cpx_atanh" (z AS Complex) AS Complex
