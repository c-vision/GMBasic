' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' scheduling.inc — namespace OPTIMIZE: cammini con finestre temporali (pacchetto
' gmbscheduling). Grafo orientato con una finestra [start, end] per nodo: l'arco u->v è
' percorribile solo se end[u] + minConnect <= start[v].
'
' windows() = start(n) | end(n) | minConnect  -> 2*n+1 valori.
' paths()   = maxPaths*n (ogni riga: fino a n indici di nodo), lens() = maxPaths.
' Ritorna il numero di cammini trovati (0 se nessuno), -1 su argomenti invalidi.

NAMESPACE OPTIMIZE

' SCHEDPATHSTIMEWINDOWS: cammini s->t che rispettano le finestre temporali.
DECLARE FUNCTION SCHEDPATHSTIMEWINDOWS LIB "gmbscheduling" ALIAS "ofx_sched_paths_time_window" (adj() AS LONG, windows() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG, paths() AS LONG, lens() AS LONG, maxPaths AS LONG) AS LONG
