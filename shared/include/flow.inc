' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' flow.inc — namespace GRAPH: FLUSSI DI RETE e MATCHING (pacchetto FFI gmbflow).
'
' Matrici dense n×n row-major. Capacità: DOUBLE >= 0 (0 = arco assente).
' Gli array out() vanno dimensionati dal chiamante:
'   - side()/match()/parent(): n
'   - flow(): n*n
'   - weight(): n
'   - flowValue(): 1
'
' I nomi sono specifici (MAXFLOW, MINCUT, DINIC, ...) per non collidere con
' l'IMPORT wildcard di GRAPH/OPTIMIZE/BRAINS.

NAMESPACE GRAPH

' ── Max-flow / min-cut ───────────────────────────────────────────────────────
' Max-flow (Edmonds-Karp): capacità cap(i*n+j); ritorna il valore (-1 errore).
DECLARE FUNCTION MAXFLOW LIB "gmbflow" ALIAS "ffx_max_flow__ffi" (cap() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG) AS DOUBLE
' Min-cut: side() = 0 lato sorgente, 1 lato pozzo; ritorna il valore del taglio.
DECLARE FUNCTION MINCUT LIB "gmbflow" ALIAS "ffx_min_cut__ffi" (cap() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG, side() AS LONG) AS DOUBLE
' Dinic (max-flow veloce): ritorna il valore.
DECLARE FUNCTION DINIC LIB "gmbflow" ALIAS "ffx_dinic__ffi" (cap() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG) AS DOUBLE

' ── Min-cost flow ────────────────────────────────────────────────────────────
' Costo minimo per il max-flow; flowValue(0) riceve il valore del flusso.
DECLARE FUNCTION MINCOSTFLOW LIB "gmbflow" ALIAS "ffx_min_cost_flow__ffi" (cap() AS DOUBLE, cost() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG, flowValue() AS DOUBLE) AS DOUBLE

' ── Matching ─────────────────────────────────────────────────────────────────
' Matching bipartito massimo (Kuhn): adj() = nL*nR; ritorna la cardinalità.
DECLARE FUNCTION BIPARTITEMATCH LIB "gmbflow" ALIAS "ffx_bip_match__ffi" (adj() AS LONG, nL AS LONG, nR AS LONG, matchL() AS LONG, matchR() AS LONG) AS LONG
' Matching massimo su grafo generale (blossom): match() = n; ritorna la cardinalità.
DECLARE FUNCTION GENERALMATCH LIB "gmbflow" ALIAS "ffx_gen_match__ffi" (adj() AS LONG, n AS LONG, match() AS LONG) AS LONG
' Matching pesato (euristica greedy + local search): ritorna il peso totale.
DECLARE FUNCTION WEIGHTEDMATCH LIB "gmbflow" ALIAS "ffx_weighted_match__ffi" (w() AS DOUBLE, n AS LONG, match() AS LONG) AS DOUBLE

' ── Circolazione / cut tree / multicommodity ─────────────────────────────────
' Circolazione ammissibile con lower/upper e domanda netta b(): 1 se ammissibile.
DECLARE FUNCTION CIRCULATION LIB "gmbflow" ALIAS "ffx_circulation__ffi" (lower() AS DOUBLE, upper() AS DOUBLE, n AS LONG, b() AS DOUBLE, flow() AS DOUBLE) AS LONG
' Gomory-Hu tree: parent() = n, weight() = n; ritorna n-1.
DECLARE FUNCTION GOMORYHU LIB "gmbflow" ALIAS "ffx_gomory_hu__ffi" (w() AS DOUBLE, n AS LONG, parent() AS LONG, weight() AS DOUBLE) AS LONG
' Multi-commodity flow (greedy): commodities() = ncomm*3 (src, dst, demand); flow() = n*n.
DECLARE FUNCTION MULTICOMMODITY LIB "gmbflow" ALIAS "ffx_multicommodity__ffi" (cap() AS DOUBLE, n AS LONG, ncomm AS LONG, commodities() AS DOUBLE, flow() AS DOUBLE) AS DOUBLE
