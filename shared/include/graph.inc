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

' ── Algoritmi avanzati (2ª slice) ────────────────────────────────────────────
' Ponti (grafo non orientato): is_bridge() = n*n (0/1); ritorna #ponti.
DECLARE FUNCTION BRIDGES LIB "gmbgraph" ALIAS "gfx_bridges" (adj() AS LONG, n AS LONG, is_bridge() AS LONG) AS LONG
' Punti di articolazione: art() = n (0/1); ritorna il numero.
DECLARE FUNCTION ARTICULATION LIB "gmbgraph" ALIAS "gfx_articulation" (adj() AS LONG, n AS LONG, art() AS LONG) AS LONG
' A*: h() euristica ammissibile; path() = n; info(0)=plen, info(1)=1 se trovato.
' Ritorna il costo del cammino o -1 se irraggiungibile.
DECLARE FUNCTION ASTAR LIB "gmbgraph" ALIAS "gfx_astar" (w() AS DOUBLE, n AS LONG, h() AS DOUBLE, s AS LONG, t AS LONG, path() AS LONG, info() AS LONG) AS DOUBLE
' Global min-cut (Stoer-Wagner, matrice SIMMETRICA).
DECLARE FUNCTION STOERWAGNER LIB "gmbgraph" ALIAS "gfx_stoer_wagner" (w() AS DOUBLE, n AS LONG) AS DOUBLE
' K cammini minimi semplici (Yen): paths() = K*n, plens() = K; ritorna K'.
DECLARE FUNCTION KSHORTEST LIB "gmbgraph" ALIAS "gfx_k_shortest" (w() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG, K AS LONG, paths() AS LONG, plens() AS LONG) AS LONG
' Isomorfismo (VF2): mapping() = n1; 1 se isomorfi.
DECLARE FUNCTION ISOMORPHIC LIB "gmbgraph" ALIAS "gfx_isomorphic" (adj1() AS LONG, n1 AS LONG, adj2() AS LONG, n2 AS LONG, mapping() AS LONG) AS LONG
' DRNL (SEAL) per il link (u,v): label() = n; ritorna 1 se calcolato.
DECLARE FUNCTION DRNL LIB "gmbgraph" ALIAS "gfx_drnl" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG, label() AS LONG) AS LONG
' Weisfeiler-Lehman (1-WL): colors() = n; ritorna #classi.
DECLARE FUNCTION WL LIB "gmbgraph" ALIAS "gfx_wl" (adj() AS LONG, n AS LONG, colors() AS LONG) AS LONG
' Min path cover di un DAG.
DECLARE FUNCTION MINPATHCOVER LIB "gmbgraph" ALIAS "gfx_min_path_cover" (adj() AS LONG, n AS LONG) AS LONG
' BFS direction-optimizing (Beamer): dist() = n (-1 non raggiunto); ritorna #visitati.
DECLARE FUNCTION BFSOPT LIB "gmbgraph" ALIAS "gfx_bfs_direction_optimizing" (adj() AS LONG, n AS LONG, src AS LONG, thr AS DOUBLE, dist() AS DOUBLE) AS LONG

' ── Link prediction (baseline, gmbortools `gor_linkpred`) ────────────────────
' Punteggi per la probabilità che esista l'arco (u,v) in un grafo denso n×n
' (adj() 0/1 oppure pesata per KATZ). Complementari a DRNL/WL (già sopra).
' Vicini in comune di u e v.
DECLARE FUNCTION LINKCN LIB "gmbgraph" ALIAS "gfx_link_common_neighbors" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS LONG
' Jaccard: |CN| / |Γ(u) ∪ Γ(v)|.
DECLARE FUNCTION LINKJACCARD LIB "gmbgraph" ALIAS "gfx_link_jaccard" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS DOUBLE
' Adamic-Adar: Σ_{w ∈ CN} 1/log(deg(w)).
DECLARE FUNCTION LINKADAMICADAR LIB "gmbgraph" ALIAS "gfx_link_adamic_adar" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS DOUBLE
' Resource Allocation: Σ_{w ∈ CN} 1/deg(w).
DECLARE FUNCTION LINKRESOURCEALLOC LIB "gmbgraph" ALIAS "gfx_link_resource_allocation" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS DOUBLE
' Preferential Attachment: deg(u)*deg(v).
DECLARE FUNCTION LINKPREFATTACH LIB "gmbgraph" ALIAS "gfx_link_preferential_attachment" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS LONG
' Katz: Σ_{l≥1} β^l (A^l)_{uv}; w() è la matrice DOUBLE pesata. 0.0 su errore.
DECLARE FUNCTION LINKKATZ LIB "gmbgraph" ALIAS "gfx_link_katz" (w() AS DOUBLE, n AS LONG, beta AS DOUBLE, u AS LONG, v AS LONG) AS DOUBLE

' ── Sentinella ───────────────────────────────────────────────────────────────
' BASIC non ha INFINITY: per "arco assente" usare GRAPH.ABSENT() nei vettori DOUBLE.
' Lo shim normalizza qualunque peso >= 1e29 a INFINITY (sentinella di gmbortools).
PUBLIC FUNCTION ABSENT() AS DOUBLE
    RETURN 1000000000000000000000000000000.0
END FUNCTION
