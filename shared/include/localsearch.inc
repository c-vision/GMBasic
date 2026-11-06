' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' localsearch.inc — namespace OPTIMIZE: motore di ricerca locale (pacchetto gMBlocalsearch).
' Stato = vettore LONG di n elementi; la callback restituisce il punteggio SOFT (più alto
' è meglio), hard=0 (ammissibile). `shape`: 0=permutazione, 1=assegnazione.
' `acceptor`: 0=hill climbing, 1=late acceptance, 2=entity tabu, 3=simulated annealing,
' 4=great deluge, 5=step counting HC, 6=record-to-record.

NAMESPACE OPTIMIZE

' LSSOLVE: hill climbing, 1 partenza. out() = hard, soft, steps.
' ATTENZIONE (contratto IN/OUT): initial() è il punto di partenza in INGRESSO e riceve in
' USCITA lo stato migliore trovato (solo con esito 1). Il motore MASSIMIZZA soft.
DECLARE FUNCTION LSSOLVE LIB "gmblocalsearch" ALIAS "ofx_ls_solve__ffi" (eval AS FUNCTION(state() AS DOUBLE) AS DOUBLE, initial() AS LONG, n AS LONG, shape AS LONG, maxSteps AS LONG, out() AS DOUBLE) AS LONG
' LSSOLVECFG: sceglie partenze e acceptor (stesso contratto IN/OUT su initial()).
DECLARE FUNCTION LSSOLVECFG LIB "gmblocalsearch" ALIAS "ofx_ls_solve_cfg__ffi" (eval AS FUNCTION(state() AS DOUBLE) AS DOUBLE, initial() AS LONG, n AS LONG, shape AS LONG, maxSteps AS LONG, starts AS LONG, acceptor AS LONG, out() AS DOUBLE) AS LONG
' Costruttive su matrice distanza n*n: perm() = n.
DECLARE FUNCTION LSNEAREST LIB "gmblocalsearch" ALIAS "ofx_ls_nearest__ffi" (dist() AS DOUBLE, n AS LONG, start AS LONG, perm() AS LONG) AS LONG
DECLARE FUNCTION LSCHEAPEST LIB "gmblocalsearch" ALIAS "ofx_ls_cheapest__ffi" (dist() AS DOUBLE, n AS LONG, perm() AS LONG) AS LONG
' Millisecondi di CPU (clock C99).
DECLARE FUNCTION LSNOWMS LIB "gmblocalsearch" ALIAS "ofx_ls_now_ms" () AS DOUBLE
