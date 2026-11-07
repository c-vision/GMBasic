' FILE: opt.inc — binding FFI delle 34 funzioni del namespace OPTIMIZE (pacchetto `opt`).
' Uso: IMPORT OPTIMIZE; la dylib è `libgmbopt` (pacchetto shared/libs/opt).
'
' Differenza rispetto al builtin OPTIMIZE: i risultati ARRAY si scrivono in un
' buffer `out()` fornito dal chiamante (l'ABI FFI non restituisce array composti).
' Dimensioni: out dei minimizzatori/sistemi/LP -> n (UBOUND(x0)+1);
' SIMPLEX -> n+1 (soluzione + obiettivo); curve fitting -> np.
'
' Callback (forma FFI):
'   FUNCTION(x AS DOUBLE) AS DOUBLE                  scalari 1D
'   FUNCTION(x() AS DOUBLE) AS DOUBLE                vettore -> scalare
'   FUNCTION(x() AS DOUBLE) AS ARRAY                 vettore -> vettore (sistemi)
'   FUNCTION(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE   modello LM/CurveFit
'   FUNCTION(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY   modello LM vettoriale
'
' Le callback con RITORNO ARRAY dichiarano implicitamente m: il loro valore di ritorno è il
' numero di elementi scritti in `out` (usato da LEASTSQUARESFIT per dedurre i residui).
' `bounds`/`ranges` sono appiattiti: 2*n valori (lo0, hi0, lo1, hi1, ...).
' Le varianti senza bounds usano [0, +1e30] per LINPROG (x>=0) e [-1e30, +1e30] per i
' minimizzatori. I SUB si invocano con `CALL OPT.NOME(...)`.

NAMESPACE OPTIMIZE

' ═══════════════════════════════════════════════════════════════════════════
' Binding nativi (uso interno)
' ═══════════════════════════════════════════════════════════════════════════

DECLARE NATIVE SUB OPTN_LEASTSQUARES LIB "gmbopt" ALIAS "gor_optb_least_squares" (A() AS DOUBLE, rows AS LONG, cols AS LONG, b() AS DOUBLE, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_CHOLESKYSOLVE LIB "gmbopt" ALIAS "gor_optb_cholesky_solve" (A() AS DOUBLE, b() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_SIMPLEX LIB "gmbopt" ALIAS "gor_optb_simplex" (c() AS DOUBLE, n AS LONG, A() AS DOUBLE, m AS LONG, b() AS DOUBLE, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_LINPROG LIB "gmbopt" ALIAS "gor_optb_linprog" (c() AS DOUBLE, n AS LONG, A_ub() AS DOUBLE, m AS LONG, b_ub() AS DOUBLE, bounds() AS DOUBLE, out() AS DOUBLE)

DECLARE NATIVE SUB OPTN_NELDERMEAD LIB "gmbopt" ALIAS "gor_optb_nelder_mead" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_BFGS LIB "gmbopt" ALIAS "gor_optb_bfgs" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_LBFGS LIB "gmbopt" ALIAS "gor_optb_lbfgs" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_CONJUGATEGRADIENT LIB "gmbopt" ALIAS "gor_optb_conjugate_gradient" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_POWELL LIB "gmbopt" ALIAS "gor_optb_powell" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_TNC LIB "gmbopt" ALIAS "gor_optb_tnc" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, bounds() AS DOUBLE, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_COBYLA LIB "gmbopt" ALIAS "gor_optb_cobyla" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, bounds() AS DOUBLE, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_SLSQP LIB "gmbopt" ALIAS "gor_optb_slsqp" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, bounds() AS DOUBLE, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_MINIMIZE LIB "gmbopt" ALIAS "gor_optb_minimize" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, method AS STRING, bounds() AS DOUBLE, out() AS DOUBLE)

DECLARE NATIVE SUB OPTN_BRUTE LIB "gmbopt" ALIAS "gor_optb_brute" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, ranges() AS DOUBLE, n AS LONG, npts AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_BASINHOPPING LIB "gmbopt" ALIAS "gor_optb_basin_hopping" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, n AS LONG, niter AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_DUALANNEALING LIB "gmbopt" ALIAS "gor_optb_dual_annealing" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, bounds() AS DOUBLE, n AS LONG, maxiter AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_DIFFERENTIALEVOLUTION LIB "gmbopt" ALIAS "gor_optb_differential_evolution" (f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, bounds() AS DOUBLE, n AS LONG, out() AS DOUBLE)

DECLARE NATIVE SUB OPTN_FSOLVE LIB "gmbopt" ALIAS "gor_optb_fsolve" (f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_BROYDEN LIB "gmbopt" ALIAS "gor_optb_broyden" (f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE, n AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_LEASTSQUARESFIT LIB "gmbopt" ALIAS "gor_optb_least_squares_fit" (f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE, n AS LONG, m AS LONG, bounds() AS DOUBLE, out() AS DOUBLE)

DECLARE NATIVE SUB OPTN_LEVENBERGMARQUARDT LIB "gmbopt" ALIAS "gor_optb_levenberg_marquardt" (f AS FUNCTION(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE, p0() AS DOUBLE, np AS LONG, xdata() AS DOUBLE, ydata() AS DOUBLE, nx AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_LEVENBERGMARQUARDTVEC LIB "gmbopt" ALIAS "gor_optb_levenberg_marquardt_vec" (f AS FUNCTION(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY, p0() AS DOUBLE, np AS LONG, xdata() AS DOUBLE, ydata() AS DOUBLE, nx AS LONG, out() AS DOUBLE)
DECLARE NATIVE SUB OPTN_CURVEFIT LIB "gmbopt" ALIAS "gor_optb_curve_fit" (f AS FUNCTION(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE, xdata() AS DOUBLE, ydata() AS DOUBLE, nx AS LONG, p0() AS DOUBLE, np AS LONG, out() AS DOUBLE)


' ═══════════════════════════════════════════════════════════════════════════
' 1D — radici e minimo scalare
' ═══════════════════════════════════════════════════════════════════════════

PUBLIC DECLARE FUNCTION NEWTON LIB "gmbopt" ALIAS "gor_optb_newton" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, guess AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION BISECTION LIB "gmbopt" ALIAS "gor_optb_bisection" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION BRENT LIB "gmbopt" ALIAS "gor_optb_brent" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION SECANT LIB "gmbopt" ALIAS "gor_optb_secant" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, x0 AS DOUBLE, x1 AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION GOLDENSECTION LIB "gmbopt" ALIAS "gor_optb_golden" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION BRENTQ LIB "gmbopt" ALIAS "gor_optb_brentq" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION BRENTH LIB "gmbopt" ALIAS "gor_optb_brenth" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION RIDDER LIB "gmbopt" ALIAS "gor_optb_ridder" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION TOMS748 LIB "gmbopt" ALIAS "gor_optb_toms748" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION BRENTMIN LIB "gmbopt" ALIAS "gor_optb_brentmin" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE

' Nota: dichiarazioni DIRETTE (non wrapper BASIC) — un wrapper con parametro FUNCTION
' richiamato in un'espressione annidata ha un difetto di codegen IL (vedi bug noto).
PUBLIC DECLARE FUNCTION MINIMIZESCALAR LIB "gmbopt" ALIAS "gor_optb_minimize_scalar_default" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION MINIMIZESCALARM LIB "gmbopt" ALIAS "gor_optb_minimize_scalar" (f AS FUNCTION(x AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, method AS STRING) AS DOUBLE

' ═══════════════════════════════════════════════════════════════════════════
' Minimi quadrati lineari / Cholesky / programmazione lineare
' Le matrici sono PIATTE row-major (m*n): le DIMENSIONI le deduce il wrapper
' (rows = UBOUND(b)+1, cols = (UBOUND(A)+1)/rows per A piatta 1-D), non il
' chiamante. Per l'input JAGGED ci sono le FUNCTION omonime con `ARRAY OF DOUBLE`
' in fondo al file, che appiattiscono qui dentro.
' Una matrice 2-D (DIM m(r,c)) va passata alle DECLARE OPTN_* con dimensioni
' esplicite: un parametro array del linguaggio non conserva il rank su C99
' (IL/PCode/VM-C lo vedrebbero, C99 no) — per un'API cross-backend usare le
' forme piatte/jagged di questi wrapper.
' ═══════════════════════════════════════════════════════════════════════════

' A (rows×cols, row-major), b[rows] -> out[cols].
PUBLIC SUB LEASTSQUARES(A() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE)
    DIM rows AS LONG = UBOUND(b) + 1
    DIM cols AS LONG = (UBOUND(A) + 1) / rows
    CALL OPTN_LEASTSQUARES(A, rows, cols, b, out)
END SUB

' A (n×n simmetrica definita positiva, row-major), b[n] -> out[n].
PUBLIC SUB CHOLESKYSOLVE(A() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(b) + 1
    CALL OPTN_CHOLESKYSOLVE(A, b, n, out)
END SUB

' SIMPLEX: massimizza c'x con A x <= b, x >= 0 (A m×n row-major).
' out(n) = soluzione, out(n+1) = obiettivo.
PUBLIC SUB SIMPLEX(c() AS DOUBLE, A() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(c) + 1
    DIM m AS LONG = UBOUND(b) + 1
    CALL OPTN_SIMPLEX(c, n, A, m, b, out)
END SUB

' LINPROG minimizza c'x; A x <= b (m×n row-major), x >= 0.
PUBLIC SUB LINPROG(c() AS DOUBLE, A() AS DOUBLE, b() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(c) + 1
    DIM m AS LONG = UBOUND(b) + 1
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG
    FOR i = 0 TO n - 1
        bnd(2 * i) = 0.0 : bnd(2 * i + 1) = big
    NEXT i
    CALL OPTN_LINPROG(c, n, A, m, b, bnd, out)
END SUB

PUBLIC SUB LINPROGB(c() AS DOUBLE, A() AS DOUBLE, b() AS DOUBLE, bounds() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(c) + 1
    DIM m AS LONG = UBOUND(b) + 1
    CALL OPTN_LINPROG(c, n, A, m, b, bounds, out)
END SUB
' LEASTSQUARESFIT con m DEDOTTO: la callback dichiara quanti residui restituisce.
DECLARE NATIVE SUB OPTN_LEASTSQUARESFITAUTO LIB "gmbopt" ALIAS "gor_optb_least_squares_fit_auto" (f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE, n AS LONG, bounds() AS DOUBLE, out() AS DOUBLE)

' Uguaglianze Ae x = be (me×n) espanse in due disuguaglianze.
PUBLIC SUB LINPROGEQ(c() AS DOUBLE, A() AS DOUBLE, b() AS DOUBLE, Ae() AS DOUBLE, be() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(c) + 1
    DIM m AS LONG = UBOUND(b) + 1
    DIM me AS LONG = UBOUND(be) + 1
    DIM rows AS LONG = m + 2 * me
    DIM flat(rows * n - 1) AS DOUBLE
    DIM bb(rows - 1) AS DOUBLE
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO n - 1
        bnd(2 * i) = 0.0 : bnd(2 * i + 1) = big
    NEXT i
    FOR i = 0 TO m - 1
        bb(i) = b(i)
        FOR j = 0 TO n - 1
            flat(i * n + j) = A(i * n + j)
        NEXT j
    NEXT i
    FOR i = 0 TO me - 1
        bb(m + 2 * i) = be(i)
        bb(m + 2 * i + 1) = -be(i)
        FOR j = 0 TO n - 1
            flat((m + 2 * i) * n + j) = Ae(i * n + j)
            flat((m + 2 * i + 1) * n + j) = -Ae(i * n + j)
        NEXT j
    NEXT i
    CALL OPTN_LINPROG(c, n, flat, rows, bb, bnd, out)
END SUB

PUBLIC SUB LINPROGFULL(c() AS DOUBLE, A() AS DOUBLE, b() AS DOUBLE, Ae() AS DOUBLE, be() AS DOUBLE, bounds() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(c) + 1
    DIM m AS LONG = UBOUND(b) + 1
    DIM me AS LONG = UBOUND(be) + 1
    DIM rows AS LONG = m + 2 * me
    DIM flat(rows * n - 1) AS DOUBLE
    DIM bb(rows - 1) AS DOUBLE
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO m - 1
        bb(i) = b(i)
        FOR j = 0 TO n - 1
            flat(i * n + j) = A(i * n + j)
        NEXT j
    NEXT i
    FOR i = 0 TO me - 1
        bb(m + 2 * i) = be(i)
        bb(m + 2 * i + 1) = -be(i)
        FOR j = 0 TO n - 1
            flat((m + 2 * i) * n + j) = Ae(i * n + j)
            flat((m + 2 * i + 1) * n + j) = -Ae(i * n + j)
        NEXT j
    NEXT i
    CALL OPTN_LINPROG(c, n, flat, rows, bb, bounds, out)
END SUB

' ═══════════════════════════════════════════════════════════════════════════
' Minimizzazione multivariata (out = n)
' ═══════════════════════════════════════════════════════════════════════════

PUBLIC SUB NELDERMEAD(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_NELDERMEAD(f, x0, UBOUND(x0) + 1, out)
END SUB

PUBLIC SUB BFGS(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_BFGS(f, x0, UBOUND(x0) + 1, out)
END SUB

PUBLIC SUB LBFGS(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_LBFGS(f, x0, UBOUND(x0) + 1, out)
END SUB

PUBLIC SUB CONJUGATEGRADIENT(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_CONJUGATEGRADIENT(f, x0, UBOUND(x0) + 1, out)
END SUB

PUBLIC SUB POWELL(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_POWELL(f, x0, UBOUND(x0) + 1, out)
END SUB

PUBLIC SUB TNC(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(x0) + 1
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG
    FOR i = 0 TO n - 1
        bnd(2 * i) = -big : bnd(2 * i + 1) = big
    NEXT i
    CALL OPTN_TNC(f, x0, n, bnd, out)
END SUB

PUBLIC SUB TNCB(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, bounds() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_TNC(f, x0, UBOUND(x0) + 1, bounds, out)
END SUB

PUBLIC SUB COBYLA(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(x0) + 1
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG
    FOR i = 0 TO n - 1
        bnd(2 * i) = -big : bnd(2 * i + 1) = big
    NEXT i
    CALL OPTN_COBYLA(f, x0, n, bnd, out)
END SUB

PUBLIC SUB COBYLAB(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, bounds() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_COBYLA(f, x0, UBOUND(x0) + 1, bounds, out)
END SUB

PUBLIC SUB SLSQP(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(x0) + 1
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG
    FOR i = 0 TO n - 1
        bnd(2 * i) = -big : bnd(2 * i + 1) = big
    NEXT i
    CALL OPTN_SLSQP(f, x0, n, bnd, out)
END SUB

PUBLIC SUB SLSQPB(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, bounds() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_SLSQP(f, x0, UBOUND(x0) + 1, bounds, out)
END SUB

PUBLIC SUB MINIMIZE(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(x0) + 1
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG
    FOR i = 0 TO n - 1
        bnd(2 * i) = -big : bnd(2 * i + 1) = big
    NEXT i
    CALL OPTN_MINIMIZE(f, x0, n, "nelder-mead", bnd, out)
END SUB

PUBLIC SUB MINIMIZEM(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, method AS STRING, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(x0) + 1
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG
    FOR i = 0 TO n - 1
        bnd(2 * i) = -big : bnd(2 * i + 1) = big
    NEXT i
    CALL OPTN_MINIMIZE(f, x0, n, method, bnd, out)
END SUB

PUBLIC SUB MINIMIZEB(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, method AS STRING, bounds() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_MINIMIZE(f, x0, UBOUND(x0) + 1, method, bounds, out)
END SUB

' ═══════════════════════════════════════════════════════════════════════════
' Globali (out = n)
' ═══════════════════════════════════════════════════════════════════════════

PUBLIC SUB BRUTE(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, ranges() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_BRUTE(f, ranges, (UBOUND(ranges) + 1) / 2, 20, out)
END SUB

PUBLIC SUB BRUTEN(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, ranges() AS DOUBLE, N AS LONG, out() AS DOUBLE)
    CALL OPTN_BRUTE(f, ranges, (UBOUND(ranges) + 1) / 2, N, out)
END SUB

PUBLIC SUB BASINHOPPING(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_BASINHOPPING(f, x0, UBOUND(x0) + 1, 50, out)
END SUB

PUBLIC SUB BASINHOPPINGN(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, niter AS LONG, out() AS DOUBLE)
    CALL OPTN_BASINHOPPING(f, x0, UBOUND(x0) + 1, niter, out)
END SUB

PUBLIC SUB DUALANNEALING(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, bounds() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_DUALANNEALING(f, bounds, (UBOUND(bounds) + 1) / 2, 1000, out)
END SUB

PUBLIC SUB DUALANNEALINGN(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, bounds() AS DOUBLE, maxiter AS LONG, out() AS DOUBLE)
    CALL OPTN_DUALANNEALING(f, bounds, (UBOUND(bounds) + 1) / 2, maxiter, out)
END SUB

PUBLIC SUB DIFFERENTIALEVOLUTION(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, bounds() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_DIFFERENTIALEVOLUTION(f, bounds, (UBOUND(bounds) + 1) / 2, out)
END SUB

' ═══════════════════════════════════════════════════════════════════════════
' Sistemi e minimi quadrati non lineari
' ═══════════════════════════════════════════════════════════════════════════

PUBLIC SUB FSOLVE(f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_FSOLVE(f, x0, UBOUND(x0) + 1, out)
END SUB

PUBLIC SUB BROYDEN(f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_BROYDEN(f, x0, UBOUND(x0) + 1, out)
END SUB

' m = numero di residui = UBOUND(out) + 1 (deve essere >= n).
PUBLIC SUB LEASTSQUARESFIT(f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE, out() AS DOUBLE)
    DIM n AS LONG = UBOUND(x0) + 1
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG
    FOR i = 0 TO n - 1
        bnd(2 * i) = -big : bnd(2 * i + 1) = big
    NEXT i
    CALL OPTN_LEASTSQUARESFIT(f, x0, n, UBOUND(out) + 1, bnd, out)
END SUB

' CONTRATTO di out(): dimensione = NUMERO DI RESIDUI m restituiti dalla callback; i parametri
' stimati si leggono in out(0..n-1). Con out() piu' corto di m il motore non converge (IL/PCode
' lo rifiutano, C99/VM-C tolleravano): vedi mt41_leastsquaresfitb.bas.
PUBLIC SUB LEASTSQUARESFITB(f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE, bounds() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_LEASTSQUARESFIT(f, x0, UBOUND(x0) + 1, UBOUND(out) + 1, bounds, out)
END SUB

' ═══════════════════════════════════════════════════════════════════════════
' Curve fitting (out = np)
' ═══════════════════════════════════════════════════════════════════════════

PUBLIC SUB LEVENBERGMARQUARDT(f AS FUNCTION(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE, p0() AS DOUBLE, xdata() AS DOUBLE, ydata() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_LEVENBERGMARQUARDT(f, p0, UBOUND(p0) + 1, xdata, ydata, UBOUND(xdata) + 1, out)
END SUB

PUBLIC SUB LEVENBERGMARQUARDTVEC(f AS FUNCTION(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY, p0() AS DOUBLE, xdata() AS DOUBLE, ydata() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_LEVENBERGMARQUARDTVEC(f, p0, UBOUND(p0) + 1, xdata, ydata, UBOUND(xdata) + 1, out)
END SUB

PUBLIC SUB CURVEFIT(f AS FUNCTION(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE, xdata() AS DOUBLE, ydata() AS DOUBLE, p0() AS DOUBLE, out() AS DOUBLE)
    CALL OPTN_CURVEFIT(f, xdata, ydata, UBOUND(xdata) + 1, p0, UBOUND(p0) + 1, out)
END SUB



' ═══════════════════════════════════════════════════════════════════════════
' Forma DROP-IN del builtin `OPTIMIZE.*` (2026-11-03): FUNCTION omonime che
' ritornano l'array, così il porting è `DIM x() = OPT.BFGS(f, x0)` come col
' builtin. Le SUB omonime (stesso nome, un argomento `out()` in più) restano
' per la forma esplicita con buffer. Le dimensioni del risultato sono quelle
' documentate per ogni funzione (out = n, n+1 per SIMPLEX, np per il fitting).
' `LEASTSQUARESFIT` deduce m dal valore di ritorno della callback (come il builtin).
' ═══════════════════════════════════════════════════════════════════════════

' out = soluzione (n valori) + obiettivo in coda (n+1 totali), come la SUB.
PUBLIC FUNCTION LINPROGB(c() AS DOUBLE, A() AS DOUBLE, b() AS DOUBLE, bounds() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(c) + 1
    DIM o(n - 1) AS DOUBLE
    CALL LINPROGB(c, A, b, bounds, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION NELDERMEAD(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL NELDERMEAD(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION BFGS(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL BFGS(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION LBFGS(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL LBFGS(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION CONJUGATEGRADIENT(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL CONJUGATEGRADIENT(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION POWELL(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL POWELL(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TNC(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL TNC(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION TNCB(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, bounds() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL TNCB(f, x0, bounds, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION COBYLA(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL COBYLA(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION COBYLAB(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, bounds() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL COBYLAB(f, x0, bounds, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION SLSQP(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL SLSQP(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION SLSQPB(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, bounds() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL SLSQPB(f, x0, bounds, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION MINIMIZE(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL MINIMIZE(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION MINIMIZEM(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, method AS STRING) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL MINIMIZEM(f, x0, method, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION MINIMIZEB(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, method AS STRING, bounds() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL MINIMIZEB(f, x0, method, bounds, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION BRUTE(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, ranges() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o((UBOUND(ranges) + 1) / 2 - 1) AS DOUBLE
    CALL BRUTE(f, ranges, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION BRUTEN(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, ranges() AS DOUBLE, N AS LONG) AS ARRAY OF DOUBLE
    DIM o((UBOUND(ranges) + 1) / 2 - 1) AS DOUBLE
    CALL BRUTEN(f, ranges, N, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION BASINHOPPING(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL BASINHOPPING(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION BASINHOPPINGN(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x0() AS DOUBLE, niter AS LONG) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL BASINHOPPINGN(f, x0, niter, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION DUALANNEALING(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, bounds() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o((UBOUND(bounds) + 1) / 2 - 1) AS DOUBLE
    CALL DUALANNEALING(f, bounds, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION DUALANNEALINGN(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, bounds() AS DOUBLE, maxiter AS LONG) AS ARRAY OF DOUBLE
    DIM o((UBOUND(bounds) + 1) / 2 - 1) AS DOUBLE
    CALL DUALANNEALINGN(f, bounds, maxiter, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION DIFFERENTIALEVOLUTION(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, bounds() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o((UBOUND(bounds) + 1) / 2 - 1) AS DOUBLE
    CALL DIFFERENTIALEVOLUTION(f, bounds, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION FSOLVE(f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL FSOLVE(f, x0, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION BROYDEN(f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL BROYDEN(f, x0, o)
    RETURN o
END FUNCTION

' LEASTSQUARESFIT: m = numero di residui, dedotto dal valore di ritorno della callback
' (stessa semantica del builtin OPTIMIZE.LEASTSQUARESFIT).
PUBLIC FUNCTION LEASTSQUARESFIT(f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(x0) + 1
    DIM o(n - 1) AS DOUBLE
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG
    FOR i = 0 TO n - 1
        bnd(2 * i) = -big : bnd(2 * i + 1) = big
    NEXT i
    CALL OPTN_LEASTSQUARESFITAUTO(f, x0, n, bnd, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION LEASTSQUARESFITB(f AS FUNCTION(x() AS DOUBLE) AS ARRAY, x0() AS DOUBLE, bounds() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(x0)) AS DOUBLE
    CALL LEASTSQUARESFITB(f, x0, bounds, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION LEVENBERGMARQUARDT(f AS FUNCTION(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE, p0() AS DOUBLE, xdata() AS DOUBLE, ydata() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(p0)) AS DOUBLE
    CALL LEVENBERGMARQUARDT(f, p0, xdata, ydata, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION LEVENBERGMARQUARDTVEC(f AS FUNCTION(p() AS DOUBLE, x() AS DOUBLE) AS ARRAY, p0() AS DOUBLE, xdata() AS DOUBLE, ydata() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(p0)) AS DOUBLE
    CALL LEVENBERGMARQUARDTVEC(f, p0, xdata, ydata, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION CURVEFIT(f AS FUNCTION(p() AS DOUBLE, x AS DOUBLE) AS DOUBLE, xdata() AS DOUBLE, ydata() AS DOUBLE, p0() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM o(UBOUND(p0)) AS DOUBLE
    CALL CURVEFIT(f, xdata, ydata, p0, o)
    RETURN o
END FUNCTION

' ── Forme JAGGED (compatibili col builtin: nessuna dimensione esplicita) ────────────────
' Acceptano `DIM A(rows) AS ARRAY OF DOUBLE` come `OPTIMIZE.*` e appiattiscono qui dentro.

PUBLIC FUNCTION LEASTSQUARES(A() AS ARRAY OF DOUBLE, b() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM rows AS LONG = UBOUND(A) + 1
    DIM cols AS LONG = UBOUND(A(0)) + 1
    DIM flat(rows * cols - 1) AS DOUBLE
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO rows - 1
        FOR j = 0 TO cols - 1
            flat(i * cols + j) = A(i)(j)
        NEXT j
    NEXT i
    DIM o(cols - 1) AS DOUBLE
    CALL OPTN_LEASTSQUARES(flat, rows, cols, b, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION CHOLESKYSOLVE(A() AS ARRAY OF DOUBLE, b() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(A) + 1
    DIM flat(n * n - 1) AS DOUBLE
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO n - 1
        FOR j = 0 TO n - 1
            flat(i * n + j) = A(i)(j)
        NEXT j
    NEXT i
    DIM o(n - 1) AS DOUBLE
    CALL OPTN_CHOLESKYSOLVE(flat, b, n, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION SIMPLEX(c() AS DOUBLE, A() AS ARRAY OF DOUBLE, b() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(c) + 1
    DIM m AS LONG = UBOUND(A) + 1
    DIM flat(m * n - 1) AS DOUBLE
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO m - 1
        FOR j = 0 TO n - 1
            flat(i * n + j) = A(i)(j)
        NEXT j
    NEXT i
    DIM o(n) AS DOUBLE
    CALL OPTN_SIMPLEX(c, n, flat, m, b, o)
    RETURN o
END FUNCTION

PUBLIC FUNCTION LINPROG(c() AS DOUBLE, A() AS ARRAY OF DOUBLE, b() AS DOUBLE) AS ARRAY OF DOUBLE
    DIM n AS LONG = UBOUND(c) + 1
    DIM m AS LONG = UBOUND(A) + 1
    DIM flat(m * n - 1) AS DOUBLE
    DIM bnd(2 * n - 1) AS DOUBLE
    DIM big AS DOUBLE = 10.0 ^ 30
    DIM i AS LONG, j AS LONG
    FOR i = 0 TO m - 1
        FOR j = 0 TO n - 1
            flat(i * n + j) = A(i)(j)
        NEXT j
    NEXT i
    FOR i = 0 TO n - 1
        bnd(2 * i) = 0.0 : bnd(2 * i + 1) = big
    NEXT i
    DIM o(n - 1) AS DOUBLE
    CALL OPTN_LINPROG(c, n, flat, m, b, bnd, o)
    RETURN o
END FUNCTION
