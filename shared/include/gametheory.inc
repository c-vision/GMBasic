' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' gametheory.inc — namespace OPTIMIZE: teoria dei giochi (pacchetto FFI gmbgametheory).
' Matrici dei pagamenti row-major m×n.

NAMESPACE OPTIMIZE

' Saddle point puro: out(0)=valore, out(1)=riga, out(2)=colonna; 1 se esiste.
DECLARE FUNCTION GAMESADDLE LIB "gmbgametheory" ALIAS "ofx_game_saddle__ffi" (payoff() AS DOUBLE, m AS LONG, n AS LONG, out() AS DOUBLE) AS LONG
' Valore misto (von Neumann): rowStrat() = m, colStrat() = n.
DECLARE FUNCTION GAMEVALUE LIB "gmbgametheory" ALIAS "ofx_game_value__ffi" (payoff() AS DOUBLE, m AS LONG, n AS LONG, rowStrat() AS DOUBLE, colStrat() AS DOUBLE) AS DOUBLE
' Minimax (Brown-Robinson): out() = valore | p[m] | q[n].
DECLARE FUNCTION GAMEMINIMAX LIB "gmbgametheory" ALIAS "ofx_game_minimax__ffi" (A() AS DOUBLE, m AS LONG, n AS LONG, out() AS DOUBLE) AS LONG
' Nash per giochi bimatrice: out() = p[m] | q[n].
DECLARE FUNCTION NASH2P LIB "gmbgametheory" ALIAS "ofx_nash_2p__ffi" (A() AS DOUBLE, B() AS DOUBLE, m AS LONG, n AS LONG, out() AS DOUBLE) AS LONG
' Shapley: v(mask) = valore della coalizione; phi() = n.
DECLARE FUNCTION SHAPLEY LIB "gmbgametheory" ALIAS "ofx_shapley__ffi" (v AS FUNCTION(mask AS DOUBLE) AS DOUBLE, n AS LONG, phi() AS DOUBLE) AS LONG
