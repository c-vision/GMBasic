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
DECLARE FUNCTION LSSOLVE LIB "gmblocalsearch" ALIAS "ofx_ls_solve" (eval AS FUNCTION(state() AS DOUBLE) AS DOUBLE, initial() AS LONG, n AS LONG, shape AS LONG, maxSteps AS LONG, out() AS DOUBLE) AS LONG
' LSSOLVECFG: sceglie partenze e acceptor.
DECLARE FUNCTION LSSOLVECFG LIB "gmblocalsearch" ALIAS "ofx_ls_solve_cfg" (eval AS FUNCTION(state() AS DOUBLE) AS DOUBLE, initial() AS LONG, n AS LONG, shape AS LONG, maxSteps AS LONG, starts AS LONG, acceptor AS LONG, out() AS DOUBLE) AS LONG
' Costruttive su matrice distanza n*n: perm() = n.
DECLARE FUNCTION LSNEAREST LIB "gmblocalsearch" ALIAS "ofx_ls_nearest" (dist() AS DOUBLE, n AS LONG, start AS LONG, perm() AS LONG) AS LONG
DECLARE FUNCTION LSCHEAPEST LIB "gmblocalsearch" ALIAS "ofx_ls_cheapest" (dist() AS DOUBLE, n AS LONG, perm() AS LONG) AS LONG
' Millisecondi di CPU (clock C99).
DECLARE FUNCTION LSNOWMS LIB "gmblocalsearch" ALIAS "ofx_ls_now_ms" () AS DOUBLE
