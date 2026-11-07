' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' lagrangian.inc — namespace OPTIMIZE: rilassamento lagrangiano con subgradiente
' (pacchetto FFI gmblagrangian). Problema: min f(x) s.t. A x >= b, x in X.
' I vincoli sono rilassati con moltiplicatori lambda >= 0; il subproblema su X è
' risolto dall'utente tramite callback.
'
' CALLBACK (forma "ritorno ARRAY"): riceve i moltiplicatori lam() e restituisce un
' vettore di nx+1 valori: r(0) = valore ottimo del subproblema, r(1..nx) = x(0..nx-1).
' La soluzione x serve al motore per il subgradiente b - A x e per tenere la migliore x.
'
' A è m×nx row-major. out() = [lower bound, lambda(m), best x(nx)] = 1+m+nx valori;
' x è il punto del subproblema che ha prodotto il miglior bound (può essere NON ammissibile,
' è il bound a essere valido). Ritorno = out(0), l'upper bound del rilassamento.

NAMESPACE OPTIMIZE

' LAGRANGIANSUBGRADIENT: maxit iterazioni (<=0 -> 100). Ritorna il miglior lower bound.
DECLARE NATIVE FUNCTION LAGRANGIANSUBGRADIENT LIB "gmblagrangian" ALIAS "ofx_lagr_subgradient" (sub AS FUNCTION(lam() AS DOUBLE) AS ARRAY, A() AS DOUBLE, b() AS DOUBLE, m AS LONG, nx AS LONG, maxit AS LONG, out() AS DOUBLE) AS DOUBLE
