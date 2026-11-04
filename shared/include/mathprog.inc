' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' mathprog.inc — namespace OPTIMIZE: programmazione matematica (pacchetto FFI gmbmathprog).
' LP/MILP (simplesso revised/duale, bounds, Gomory, B&B, matheuristiche) + model builder.
' Forma LP standard: min c'x s.t. A x = b, x >= 0 (A m×n row-major, b >= 0).
' Nomi BASIC senza underscore; gli array di servizio sono documentati per funzione.

NAMESPACE OPTIMIZE

' ── LP / MILP ────────────────────────────────────────────────────────────────
' LPSOLVE: x() = n, y() = m (duali), info(0) = valore ottimo. Ritorna lo status
' (0 ottimo, 1 inammissibile, 2 illimitato).
DECLARE FUNCTION LPSOLVE LIB "gmbmathprog" ALIAS "ofx_lp_solve" (A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE, y() AS DOUBLE, info() AS DOUBLE) AS LONG
' Revised simplex da base ammissibile basis() = m; x() = n.
DECLARE FUNCTION LPREVISED LIB "gmbmathprog" ALIAS "ofx_lp_revised" (A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, basis() AS LONG, x() AS DOUBLE, m AS LONG, n AS LONG) AS LONG
' LP con bound: bounds() = s[m] | b[m] | l[n] | u[n] (i bound possono essere ±1e30).
DECLARE FUNCTION LPBOUNDS LIB "gmbmathprog" ALIAS "ofx_lp_bounds" (A() AS DOUBLE, bounds() AS DOUBLE, c() AS DOUBLE, x() AS DOUBLE, m AS LONG, n AS LONG) AS LONG
' Dual simplex da base dual-ammissibile basis() = m; x() = n.
DECLARE FUNCTION LPDUAL LIB "gmbmathprog" ALIAS "ofx_lp_dual" (A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, basis() AS LONG, x() AS DOUBLE, m AS LONG, n AS LONG) AS LONG
' Costi ridotti red() = n dati i duali y() = m.
DECLARE FUNCTION LPREDUCEDCOSTS LIB "gmbmathprog" ALIAS "ofx_lp_reduced_costs" (A() AS DOUBLE, c() AS DOUBLE, y() AS DOUBLE, red() AS DOUBLE, m AS LONG, n AS LONG) AS LONG
' Ottimo alternativo: x2() = n, z2(0) = valore; 1 se esiste, 0 se unico.
DECLARE FUNCTION LPALTERNATE LIB "gmbmathprog" ALIAS "ofx_lp_alternate" (A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, x2() AS DOUBLE, m AS LONG, n AS LONG, z2() AS DOUBLE) AS LONG
' MILP branch&bound: intIdx() = nint | indici variabili intere; x() = n, info(0) = obj.
DECLARE FUNCTION MILPBB LIB "gmbmathprog" ALIAS "ofx_milp_bb" (A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, intIdx() AS LONG, x() AS DOUBLE, info() AS DOUBLE, m AS LONG, n AS LONG) AS LONG
' Matheuristic hard fixing: info(0)=fixFrac (in), info(1)=rounds (in), info(2)=obj (out).
DECLARE FUNCTION MATHHARDFIXING LIB "gmbmathprog" ALIAS "ofx_matheuristic_hard_fixing" (A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, intIdx() AS LONG, x() AS DOUBLE, info() AS DOUBLE, m AS LONG, n AS LONG) AS LONG
' Matheuristic local branching: info(0)=kStart (in), info(1)=rounds (in), info(2)=obj (out).
DECLARE FUNCTION MATHLOCALBRANCHING LIB "gmbmathprog" ALIAS "ofx_matheuristic_local_branching" (A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, intIdx() AS LONG, x() AS DOUBLE, info() AS DOUBLE, m AS LONG, n AS LONG) AS LONG
' Taglio di Gomory da una riga del tableau: tabrow() = ncols; cut() = ncols+1.
DECLARE FUNCTION GOMORYCUT LIB "gmbmathprog" ALIAS "ofx_gomory_cut" (tabrow() AS DOUBLE, ncols AS LONG, rhsCol AS LONG, cut() AS DOUBLE) AS LONG

' ── Model builder (handle LONG) ──────────────────────────────────────────────
' kind: 0=continua, 1=intera, 2=binaria. Ritorna l'indice della variabile.
DECLARE FUNCTION MODELNEW LIB "gmbmathprog" ALIAS "ofx_model_new" () AS LONG
DECLARE SUB MODELFREE LIB "gmbmathprog" ALIAS "ofx_model_free" (h AS LONG)
DECLARE FUNCTION MODELADDVAR LIB "gmbmathprog" ALIAS "ofx_model_add_var" (h AS LONG, kind AS LONG, lo AS DOUBLE, hi AS DOUBLE, name AS STRING) AS LONG
' cmp: 0=<=, 1=>=, 2==. vars()/coef() = ncoef.
DECLARE FUNCTION MODELADDCONSTR LIB "gmbmathprog" ALIAS "ofx_model_add_constr" (h AS LONG, vars() AS LONG, coef() AS DOUBLE, ncoef AS LONG, cmp AS LONG, rhs AS DOUBLE) AS LONG
' sense: 0=min, 1=max.
DECLARE SUB MODELSETOBJ LIB "gmbmathprog" ALIAS "ofx_model_set_obj" (h AS LONG, vars() AS LONG, coef() AS DOUBLE, ncoef AS LONG, sense AS LONG)
DECLARE FUNCTION MODELSOLVE LIB "gmbmathprog" ALIAS "ofx_model_solve" (h AS LONG, obj() AS DOUBLE) AS LONG
DECLARE FUNCTION MODELVARVALUE LIB "gmbmathprog" ALIAS "ofx_model_var_value" (h AS LONG, i AS LONG) AS DOUBLE
DECLARE FUNCTION MODELNUMVARS LIB "gmbmathprog" ALIAS "ofx_model_num_vars" (h AS LONG) AS LONG
DECLARE FUNCTION MODELTOJSON LIB "gmbmathprog" ALIAS "ofx_model_to_json" (h AS LONG) AS STRING
