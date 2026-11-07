' ode.inc — namespace ODE: metodi avanzati e problemi ai limiti (gmbmath-numerical, FFI).
'
' NUMERICAL.SOLVEODE/SOLVEIVP coprono un solo metodo esplicito. Qui ci sono i metodi che
' mancano:
'
'   ADAMSBASHFORTH4     multi-passo esplicito (4° ordine)
'   ADAMSMOULTON4       multi-passo implicito (4° ordine, più stabile)
'   BDF2                implicito, per problemi STIFF (maxit + tol dell'iterazione)
'   IMPLICITMIDPOINT    implicito, conservativo su sistemi Hamiltoniani lineari
'   VERLET              velocity Verlet per y'' = a(t, y) (y = posizioni, velocità)
'   BVPSHOOTING         y'' = f(x,y,y') con condizioni ai due estremi (shooting)
'   BVPFD               lo stesso problema con differenze finite
'
' Callback del sistema y' = f(y, t): `(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE`,
' dove l'array restituito ha un elemento per componente di dy/dt. L'ordine è
' **array prima, scalare dopo**: è la forma che l'ABI delle callback regge su tutte e
' quattro le gambe (descrittore 148). Un sistema autonomo ignora `t`.
' Per i BVP la callback è scalare: `(x AS DOUBLE, y AS DOUBLE, dy AS DOUBLE) AS DOUBLE`
' = y''.
'
' Lo stato NON è in ingresso/uscita: si dà y0() e si riceve yout() (stessa lunghezza),
' perché la scrittura su un array in ingresso non è portabile su tutti i backend.
' Per i BVP yout() contiene i nodi della griglia (la sua lunghezza decide quanti).
'
' Ritorno: 0 = ok, -1 = argomento non valido o metodo non convergente.

NAMESPACE ODE

' ── sistemi y' = f(y, t) ────────────────────────────────────────────────────
DECLARE FUNCTION ADAMSBASHFORTH4 LIB "gmbmath" ALIAS "ofx_ode_ab4" (f AS FUNCTION(y() AS DOUBLE, t AS DOUBLE) AS ARRAY, y0() AS DOUBLE, a AS DOUBLE, b AS DOUBLE, steps AS LONG, yout() AS DOUBLE) AS LONG
DECLARE FUNCTION ADAMSMOULTON4 LIB "gmbmath" ALIAS "ofx_ode_am4" (f AS FUNCTION(y() AS DOUBLE, t AS DOUBLE) AS ARRAY, y0() AS DOUBLE, a AS DOUBLE, b AS DOUBLE, steps AS LONG, yout() AS DOUBLE) AS LONG
' BDF2: maxit iterazioni di punto fisso interne, tol sull'incremento.
DECLARE FUNCTION BDF2 LIB "gmbmath" ALIAS "ofx_ode_bdf2" (f AS FUNCTION(y() AS DOUBLE, t AS DOUBLE) AS ARRAY, y0() AS DOUBLE, a AS DOUBLE, b AS DOUBLE, steps AS LONG, maxit AS LONG, tol AS DOUBLE, yout() AS DOUBLE) AS LONG
DECLARE FUNCTION IMPLICITMIDPOINT LIB "gmbmath" ALIAS "ofx_ode_implicit_midpoint" (f AS FUNCTION(y() AS DOUBLE, t AS DOUBLE) AS ARRAY, y0() AS DOUBLE, a AS DOUBLE, b AS DOUBLE, steps AS LONG, maxit AS LONG, tol AS DOUBLE, yout() AS DOUBLE) AS LONG
' Velocity Verlet per y'' = a(t, y): y = (posizioni, velocità), tutte nella stessa f.
DECLARE FUNCTION VERLET LIB "gmbmath" ALIAS "ofx_ode_verlet" (f AS FUNCTION(y() AS DOUBLE, t AS DOUBLE) AS ARRAY, y0() AS DOUBLE, a AS DOUBLE, b AS DOUBLE, steps AS LONG, yout() AS DOUBLE) AS LONG

' ── problemi ai limiti y'' = f(x, y, y') ────────────────────────────────────
' yout() = nodi della griglia (la lunghezza decide n); y(a) = alpha, y(b) = beta.
DECLARE FUNCTION BVPSHOOTING LIB "gmbmath" ALIAS "ofx_ode_bvp_shooting" (fn AS FUNCTION(x AS DOUBLE, y AS DOUBLE, dy AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, alpha AS DOUBLE, beta AS DOUBLE, yout() AS DOUBLE) AS LONG
DECLARE FUNCTION BVPFD LIB "gmbmath" ALIAS "ofx_ode_bvp_fd" (fn AS FUNCTION(x AS DOUBLE, y AS DOUBLE, dy AS DOUBLE) AS DOUBLE, a AS DOUBLE, b AS DOUBLE, alpha AS DOUBLE, beta AS DOUBLE, yout() AS DOUBLE) AS LONG
