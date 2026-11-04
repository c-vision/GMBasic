' ⛔ DIVIETO ASSOLUTO — NON modificare GMBasic (nessun file, nessun commit, nulla) senza
' PERMESSO ESPLICITO dell'utente. GMBasic è un progetto SEPARATO: se serve una modifica,
' FERMARSI e CHIEDERE.
'
' graph.inc — namespace GRAPH: TEORIA DEI GRAFI (pacchetto FFI gmbgraph).
'
' Rappresentazione (come gmbortools): matrice densa n×n row-major.
'   - `adj()`: LONG, 0/1 (arco assente/presente). `adj(i*n+j)` per l'arco i→j.
'   - `w()`:   DOUBLE, peso; INFINITY (1e30) = arco assente.
' `n` è il lato della matrice (numero di nodi).
'
' I grafi dei cervelli (wiki/brain) NON sono qui: sono nel namespace BRAINS
' (BRAINS.BFS/NEIGHBORS/PATH...), con le funzioni wiki-only in BRAINS.BYTAG/BRAINS.BYFOLDER/BRAINS.QUERYMULTI.
'
' Gli array `out()` vanno dimensionati dal chiamante:
'   - order()/comp()/prev()/color()/comm()/clique(): n
'   - dist()/rank()/bc()/cc(): n
'   - closure()/subgraph out(): n*n (o k*k)
'   - eu()/ev()/ew(): n-1 (MST)
' I DECLARE sono raggruppati per dominio; i nomi sono deliberatamente specifici per
' evitare collisioni con l'IMPORT wildcard di OPTIMIZE/BRAINS.

NAMESPACE GRAPH

' ── Struttura ────────────────────────────────────────────────────────────────
' Grado (out+in, archi non orientati) del nodo v; -1 se v non esiste.
DECLARE FUNCTION DEGREE LIB "gmbgraph" ALIAS "gfx_degree" (adj() AS LONG, n AS LONG, v AS LONG) AS LONG
' 1 se il grafo ORIENTATO ha un ciclo (Kahn: nodi processati < n).
DECLARE FUNCTION HASCYCLE LIB "gmbgraph" ALIAS "gfx_has_cycle" (adj() AS LONG, n AS LONG) AS LONG
' 1 se il grafo (non orientato) è bipartito.
DECLARE FUNCTION ISBIPARTITE LIB "gmbgraph" ALIAS "gfx_is_bipartite" (adj() AS LONG, n AS LONG) AS LONG

' ── Traversamento ────────────────────────────────────────────────────────────
' BFS da src: riempie order() e ritorna il numero di nodi visitati (-1 errore).
DECLARE FUNCTION BFS LIB "gmbgraph" ALIAS "gfx_bfs" (adj() AS LONG, n AS LONG, src AS LONG, order() AS LONG) AS LONG
DECLARE FUNCTION DFS LIB "gmbgraph" ALIAS "gfx_dfs" (adj() AS LONG, n AS LONG, src AS LONG, order() AS LONG) AS LONG
' Ordinamento topologico di un DAG: 1 ok (order() = nodi), 0 se c'è un ciclo.
DECLARE FUNCTION TOPOSORT LIB "gmbgraph" ALIAS "gfx_toposort" (adj() AS LONG, n AS LONG, order() AS LONG) AS LONG
' Componenti (debolmente) connesse: comp(i) = id componente; ritorna #componenti.
DECLARE FUNCTION COMPONENTS LIB "gmbgraph" ALIAS "gfx_components" (adj() AS LONG, n AS LONG, comp() AS LONG) AS LONG
' Componenti fortemente connesse (Tarjan): comp(i) = id; ritorna #componenti.
DECLARE FUNCTION SCC LIB "gmbgraph" ALIAS "gfx_scc" (adj() AS LONG, n AS LONG, comp() AS LONG) AS LONG

' ── Percorsi minimi ──────────────────────────────────────────────────────────
' Dijkstra (pesi non negativi): dist() e prev() (-1 = nessun padre). 1 ok.
DECLARE FUNCTION DIJKSTRA LIB "gmbgraph" ALIAS "gfx_dijkstra" (w() AS DOUBLE, n AS LONG, src AS LONG, dist() AS DOUBLE, prev() AS LONG) AS LONG
' Bellman-Ford (pesi negativi ammessi). 1 ok, GOR_ERR_NOCONV (4) se ciclo negativo.
DECLARE FUNCTION BELLMANFORD LIB "gmbgraph" ALIAS "gfx_bellman_ford" (w() AS DOUBLE, n AS LONG, src AS LONG, dist() AS DOUBLE, prev() AS LONG) AS LONG
' Floyd-Warshall: dist() è n*n. 1 ok.
DECLARE FUNCTION FLOYDWARSHALL LIB "gmbgraph" ALIAS "gfx_floyd_warshall" (w() AS DOUBLE, n AS LONG, dist() AS DOUBLE) AS LONG

' ── Centralità / community ───────────────────────────────────────────────────
' PageRank (damping tipico 0.85, iters 50..100). rank() = n.
DECLARE FUNCTION PAGERANK LIB "gmbgraph" ALIAS "gfx_pagerank" (w() AS DOUBLE, n AS LONG, rank() AS DOUBLE, damping AS DOUBLE, iters AS LONG) AS LONG
' Betweenness (Brandes, archi non pesati): bc() = n; ritorna il valore massimo.
DECLARE FUNCTION BETWEENNESS LIB "gmbgraph" ALIAS "gfx_betweenness" (w() AS DOUBLE, n AS LONG, bc() AS DOUBLE) AS DOUBLE
' Closeness centrality (BFS non pesato): cc() = n.
DECLARE FUNCTION CLOSENESS LIB "gmbgraph" ALIAS "gfx_closeness" (adj() AS LONG, n AS LONG, cc() AS DOUBLE) AS LONG
' Louvain (un livello): comm() = n; ritorna #community.
DECLARE FUNCTION LOUVAIN LIB "gmbgraph" ALIAS "gfx_louvain" (w() AS DOUBLE, n AS LONG, comm() AS LONG, resolution AS DOUBLE) AS LONG

' ── Colorazione / clique / chiusura ──────────────────────────────────────────
' Colorazione DSATUR: color() = 0..k-1; ritorna k.
DECLARE FUNCTION COLORING LIB "gmbgraph" ALIAS "gfx_coloring" (adj() AS LONG, n AS LONG, color() AS LONG) AS LONG
' Clique massima (Bron-Kerbosch): clique() = n; ritorna la dimensione.
DECLARE FUNCTION MAXCLIQUE LIB "gmbgraph" ALIAS "gfx_max_clique" (adj() AS LONG, n AS LONG, clique() AS LONG) AS LONG
' Chiusura transitiva (Warshall): out() = n*n, 0/1.
DECLARE FUNCTION TRANSITIVECLOSURE LIB "gmbgraph" ALIAS "gfx_transitive_closure" (adj() AS LONG, n AS LONG, out() AS LONG) AS LONG

' ── Proprietà ────────────────────────────────────────────────────────────────
' Diametro pesato (Floyd-Warshall). 0 se n<2 o nessuna coppia raggiungibile.
DECLARE FUNCTION DIAMETER LIB "gmbgraph" ALIAS "gfx_diameter" (w() AS DOUBLE, n AS LONG) AS DOUBLE
' Eccentricità del nodo v; -1 se v fuori range.
DECLARE FUNCTION ECCENTRICITY LIB "gmbgraph" ALIAS "gfx_eccentricity" (w() AS DOUBLE, n AS LONG, v AS LONG) AS DOUBLE
' Coefficiente di clustering locale di v (archi non orientati).
DECLARE FUNCTION CLUSTERING LIB "gmbgraph" ALIAS "gfx_clustering" (adj() AS LONG, n AS LONG, v AS LONG) AS DOUBLE
' Sottografo indotto dai nodi nodes(0..k-1): out() = k*k; 0 = GOR_OK.
DECLARE FUNCTION SUBGRAPH LIB "gmbgraph" ALIAS "gfx_subgraph" (w() AS DOUBLE, n AS LONG, nodes() AS LONG, k AS LONG, out() AS DOUBLE) AS LONG

' ── MST ──────────────────────────────────────────────────────────────────────
' Kruskal: riempie eu()/ev()/ew() (capacità n-1) e ritorna il numero di archi.
DECLARE FUNCTION MSTKRUSKAL LIB "gmbgraph" ALIAS "gfx_mst_kruskal" (w() AS DOUBLE, n AS LONG, eu() AS LONG, ev() AS LONG, ew() AS DOUBLE) AS LONG

' ── Sentinella ───────────────────────────────────────────────────────────────
' BASIC non ha INFINITY: per "arco assente" usare GRAPH.ABSENT() nei vettori DOUBLE.
' Lo shim normalizza qualunque peso >= 1e29 a INFINITY (sentinella di gmbortools).
PUBLIC FUNCTION ABSENT() AS DOUBLE
    RETURN 1000000000000000000000000000000.0
END FUNCTION
