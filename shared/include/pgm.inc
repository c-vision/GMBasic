' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' pgm.inc — namespace PGM: indice PGM (Piecewise Geometric Model) su chiavi ordinate.
'
' Approssima la funzione rank(x) con segmenti lineari a errore massimo `eps`
' (Swing Filter, Ferragina-Vinciguerra; motore gmbortools `gor_pgm`, da AppendPGM MIT).
' Utile per ricerca approssimata su dati ordinati: RANK stima la posizione, RANGE
' restituisce l'intervallo [lo,hi] che contiene certamente la vera posizione.
'
' L'indice è un HANDLE (massimo 16 vivi per processo, uso sequenziale):
'   h = PGM.BUILD(x(), n, eps)
'   ... PGM.RANK(h, key) / PGM.RANGE(h, key, out())
'   PGM.FREE(h)

NAMESPACE PGM

' Costruisce l'indice dalle chiavi ordinate crescenti x() (n > 0); ritorna un handle
' > 0, oppure -1 (n<=0, memoria, tabella piena: manca FREE).
DECLARE FUNCTION BUILD LIB "gmbpgm" ALIAS "gfx_pgm_build" (x() AS DOUBLE, n AS LONG, eps AS LONG) AS LONG
' Rank approssimato di key; -1 se l'indice non è valido.
DECLARE FUNCTION RANK LIB "gmbpgm" ALIAS "gfx_pgm_rank" (h AS LONG, key AS DOUBLE) AS LONG
' Come RANK, e scrive out(0)=lo, out(1)=hi (estremi garantiti, già clampati a 0..n-1).
DECLARE FUNCTION RANGE LIB "gmbpgm" ALIAS "gfx_pgm_range" (h AS LONG, key AS DOUBLE, out() AS LONG) AS LONG
' Libera l'indice; ritorna 0 su successo, -1 su handle invalido.
DECLARE FUNCTION FREE LIB "gmbpgm" ALIAS "gfx_pgm_free" (h AS LONG) AS LONG
