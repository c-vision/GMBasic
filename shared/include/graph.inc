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
' TUTTA la matematica dei grafi è qui: il namespace builtin BRAINS non contiene più
' algoritmi (BFS/DFS/Dijkstra/PageRank/Louvain/MST/SCC/TopoSort/... si chiamano da GRAPH e
' lavorano su queste matrici). In BRAINS restano solo le QUERY sul cervello
' (ADJACENCY/BYTAG/BYFOLDER/QUERYMULTI/NEIGHBORS/PATH/STATS/ISVALID/DOT). Per usare gli
' algoritmi di questo pacchetto su un cervello: BRAINS.Adjacency(brain) dà n, i nomi dei nodi
' (indice = riga/colonna) e gli archi come coppie [i,j]; da lì si riempie la matrice n x n.
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
DECLARE NATIVE FUNCTION DEGREE LIB "gmbgraph" ALIAS "gfx_degree__nat" (adj() AS LONG, n AS LONG, v AS LONG) AS LONG
' 1 se il grafo ORIENTATO ha un ciclo (Kahn: nodi processati < n).
DECLARE NATIVE FUNCTION HASCYCLE LIB "gmbgraph" ALIAS "gfx_has_cycle__nat" (adj() AS LONG, n AS LONG) AS LONG
' 1 se il grafo (non orientato) è bipartito.
DECLARE NATIVE FUNCTION ISBIPARTITE LIB "gmbgraph" ALIAS "gfx_is_bipartite__nat" (adj() AS LONG, n AS LONG) AS LONG

' ── Traversamento ────────────────────────────────────────────────────────────
' BFS da src: riempie order() e ritorna il numero di nodi visitati (-1 errore).
DECLARE NATIVE FUNCTION BFS LIB "gmbgraph" ALIAS "gfx_bfs__nat" (adj() AS LONG, n AS LONG, src AS LONG, order() AS LONG) AS LONG
DECLARE NATIVE FUNCTION DFS LIB "gmbgraph" ALIAS "gfx_dfs__nat" (adj() AS LONG, n AS LONG, src AS LONG, order() AS LONG) AS LONG
' Ordinamento topologico di un DAG: 1 ok (order() = nodi), 0 se c'è un ciclo.
DECLARE NATIVE FUNCTION TOPOSORT LIB "gmbgraph" ALIAS "gfx_toposort__nat" (adj() AS LONG, n AS LONG, order() AS LONG) AS LONG
' Componenti (debolmente) connesse: comp(i) = id componente; ritorna #componenti.
DECLARE NATIVE FUNCTION COMPONENTS LIB "gmbgraph" ALIAS "gfx_components__nat" (adj() AS LONG, n AS LONG, comp() AS LONG) AS LONG
' Componenti fortemente connesse (Tarjan): comp(i) = id; ritorna #componenti.
DECLARE NATIVE FUNCTION SCC LIB "gmbgraph" ALIAS "gfx_scc__nat" (adj() AS LONG, n AS LONG, comp() AS LONG) AS LONG

' ── Percorsi minimi ──────────────────────────────────────────────────────────
' Dijkstra (pesi non negativi): dist() e prev() (-1 = nessun padre). 1 ok.
DECLARE NATIVE FUNCTION DIJKSTRA LIB "gmbgraph" ALIAS "gfx_dijkstra__nat" (w() AS DOUBLE, n AS LONG, src AS LONG, dist() AS DOUBLE, prev() AS LONG) AS LONG
' Bellman-Ford (pesi negativi ammessi). 1 ok, GOR_ERR_NOCONV (4) se ciclo negativo.
DECLARE NATIVE FUNCTION BELLMANFORD LIB "gmbgraph" ALIAS "gfx_bellman_ford__nat" (w() AS DOUBLE, n AS LONG, src AS LONG, dist() AS DOUBLE, prev() AS LONG) AS LONG
' Floyd-Warshall: dist() è n*n. 1 ok.
DECLARE NATIVE FUNCTION FLOYDWARSHALL LIB "gmbgraph" ALIAS "gfx_floyd_warshall__nat" (w() AS DOUBLE, n AS LONG, dist() AS DOUBLE) AS LONG

' ── Centralità / community ───────────────────────────────────────────────────
' PageRank (damping tipico 0.85, iters 50..100). rank() = n.
DECLARE NATIVE FUNCTION PAGERANK LIB "gmbgraph" ALIAS "gfx_pagerank__nat" (w() AS DOUBLE, n AS LONG, rank() AS DOUBLE, damping AS DOUBLE, iters AS LONG) AS LONG
' Betweenness (Brandes, archi non pesati): bc() = n; ritorna il valore massimo.
DECLARE NATIVE FUNCTION BETWEENNESS LIB "gmbgraph" ALIAS "gfx_betweenness__nat" (w() AS DOUBLE, n AS LONG, bc() AS DOUBLE) AS DOUBLE
' Closeness centrality (BFS non pesato): cc() = n.
DECLARE NATIVE FUNCTION CLOSENESS LIB "gmbgraph" ALIAS "gfx_closeness__nat" (adj() AS LONG, n AS LONG, cc() AS DOUBLE) AS LONG
' Louvain (un livello): comm() = n; ritorna #community.
DECLARE NATIVE FUNCTION LOUVAIN LIB "gmbgraph" ALIAS "gfx_louvain__nat" (w() AS DOUBLE, n AS LONG, comm() AS LONG, resolution AS DOUBLE) AS LONG

' ── Colorazione / clique / chiusura ──────────────────────────────────────────
' Colorazione DSATUR: color() = 0..k-1; ritorna k.
DECLARE NATIVE FUNCTION COLORING LIB "gmbgraph" ALIAS "gfx_coloring__nat" (adj() AS LONG, n AS LONG, color() AS LONG) AS LONG
' Clique massima (Bron-Kerbosch): clique() = n; ritorna la dimensione.
DECLARE NATIVE FUNCTION MAXCLIQUE LIB "gmbgraph" ALIAS "gfx_max_clique__nat" (adj() AS LONG, n AS LONG, clique() AS LONG) AS LONG
' Chiusura transitiva (Warshall): out() = n*n, 0/1.
DECLARE NATIVE FUNCTION TRANSITIVECLOSURE LIB "gmbgraph" ALIAS "gfx_transitive_closure__nat" (adj() AS LONG, n AS LONG, out() AS LONG) AS LONG

' ── Proprietà ────────────────────────────────────────────────────────────────
' Diametro pesato (Floyd-Warshall). 0 se n<2 o nessuna coppia raggiungibile.
DECLARE NATIVE FUNCTION DIAMETER LIB "gmbgraph" ALIAS "gfx_diameter__nat" (w() AS DOUBLE, n AS LONG) AS DOUBLE
' Eccentricità del nodo v; -1 se v fuori range.
DECLARE NATIVE FUNCTION ECCENTRICITY LIB "gmbgraph" ALIAS "gfx_eccentricity__nat" (w() AS DOUBLE, n AS LONG, v AS LONG) AS DOUBLE
' Coefficiente di clustering locale di v (archi non orientati).
DECLARE NATIVE FUNCTION CLUSTERING LIB "gmbgraph" ALIAS "gfx_clustering__nat" (adj() AS LONG, n AS LONG, v AS LONG) AS DOUBLE
' Sottografo indotto dai nodi nodes(0..k-1): out() = k*k; 0 = GOR_OK.
DECLARE NATIVE FUNCTION SUBGRAPH LIB "gmbgraph" ALIAS "gfx_subgraph__nat" (w() AS DOUBLE, n AS LONG, nodes() AS LONG, k AS LONG, out() AS DOUBLE) AS LONG

' ── MST ──────────────────────────────────────────────────────────────────────
' Kruskal: riempie eu()/ev()/ew() (capacità n-1) e ritorna il numero di archi.
DECLARE NATIVE FUNCTION MSTKRUSKAL LIB "gmbgraph" ALIAS "gfx_mst_kruskal__nat" (w() AS DOUBLE, n AS LONG, eu() AS LONG, ev() AS LONG, ew() AS DOUBLE) AS LONG

' ── Algoritmi avanzati (2ª slice) ────────────────────────────────────────────
' Ponti (grafo non orientato): is_bridge() = n*n (0/1); ritorna #ponti.
DECLARE NATIVE FUNCTION BRIDGES LIB "gmbgraph" ALIAS "gfx_bridges__nat" (adj() AS LONG, n AS LONG, is_bridge() AS LONG) AS LONG
' Punti di articolazione: art() = n (0/1); ritorna il numero.
DECLARE NATIVE FUNCTION ARTICULATION LIB "gmbgraph" ALIAS "gfx_articulation__nat" (adj() AS LONG, n AS LONG, art() AS LONG) AS LONG
' A*: h() euristica ammissibile; path() = n; info(0)=plen, info(1)=1 se trovato.
' Ritorna il costo del cammino o -1 se irraggiungibile.
DECLARE NATIVE FUNCTION ASTAR LIB "gmbgraph" ALIAS "gfx_astar__nat" (w() AS DOUBLE, n AS LONG, h() AS DOUBLE, s AS LONG, t AS LONG, path() AS LONG, info() AS LONG) AS DOUBLE
' Global min-cut (Stoer-Wagner, matrice SIMMETRICA).
DECLARE NATIVE FUNCTION STOERWAGNER LIB "gmbgraph" ALIAS "gfx_stoer_wagner__nat" (w() AS DOUBLE, n AS LONG) AS DOUBLE
' K cammini minimi semplici (Yen): paths() = K*n, plens() = K; ritorna K'.
DECLARE NATIVE FUNCTION KSHORTEST LIB "gmbgraph" ALIAS "gfx_k_shortest__nat" (w() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG, K AS LONG, paths() AS LONG, plens() AS LONG) AS LONG
' Isomorfismo (VF2): mapping() = n1; 1 se isomorfi.
DECLARE NATIVE FUNCTION ISOMORPHIC LIB "gmbgraph" ALIAS "gfx_isomorphic__nat" (adj1() AS LONG, n1 AS LONG, adj2() AS LONG, n2 AS LONG, mapping() AS LONG) AS LONG
' DRNL (SEAL) per il link (u,v): label() = n; ritorna 1 se calcolato.
DECLARE NATIVE FUNCTION DRNL LIB "gmbgraph" ALIAS "gfx_drnl__nat" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG, label() AS LONG) AS LONG
' Weisfeiler-Lehman (1-WL): colors() = n; ritorna #classi.
DECLARE NATIVE FUNCTION WL LIB "gmbgraph" ALIAS "gfx_wl__nat" (adj() AS LONG, n AS LONG, colors() AS LONG) AS LONG
' Min path cover di un DAG.
DECLARE NATIVE FUNCTION MINPATHCOVER LIB "gmbgraph" ALIAS "gfx_min_path_cover__nat" (adj() AS LONG, n AS LONG) AS LONG
' BFS direction-optimizing (Beamer): dist() = n (-1 non raggiunto); ritorna #visitati.
DECLARE NATIVE FUNCTION BFSOPT LIB "gmbgraph" ALIAS "gfx_bfs_direction_optimizing__nat" (adj() AS LONG, n AS LONG, src AS LONG, thr AS DOUBLE, dist() AS DOUBLE) AS LONG

' ── Link prediction (baseline, gmbortools `gor_linkpred`) ────────────────────
' Punteggi per la probabilità che esista l'arco (u,v) in un grafo denso n×n
' (adj() 0/1 oppure pesata per KATZ). Complementari a DRNL/WL (già sopra).
' Vicini in comune di u e v.
DECLARE NATIVE FUNCTION LINKCN LIB "gmbgraph" ALIAS "gfx_link_common_neighbors__nat" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS LONG
' Jaccard: |CN| / |Γ(u) ∪ Γ(v)|.
DECLARE NATIVE FUNCTION LINKJACCARD LIB "gmbgraph" ALIAS "gfx_link_jaccard__nat" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS DOUBLE
' Adamic-Adar: Σ_{w ∈ CN} 1/log(deg(w)).
DECLARE NATIVE FUNCTION LINKADAMICADAR LIB "gmbgraph" ALIAS "gfx_link_adamic_adar__nat" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS DOUBLE
' Resource Allocation: Σ_{w ∈ CN} 1/deg(w).
DECLARE NATIVE FUNCTION LINKRESOURCEALLOC LIB "gmbgraph" ALIAS "gfx_link_resource_allocation__nat" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS DOUBLE
' Preferential Attachment: deg(u)*deg(v).
DECLARE NATIVE FUNCTION LINKPREFATTACH LIB "gmbgraph" ALIAS "gfx_link_preferential_attachment__nat" (adj() AS LONG, n AS LONG, u AS LONG, v AS LONG) AS LONG
' Katz: Σ_{l≥1} β^l (A^l)_{uv}; w() è la matrice DOUBLE pesata. 0.0 su errore.
DECLARE NATIVE FUNCTION LINKKATZ LIB "gmbgraph" ALIAS "gfx_link_katz__nat" (w() AS DOUBLE, n AS LONG, beta AS DOUBLE, u AS LONG, v AS LONG) AS DOUBLE

' ── Sentinella ───────────────────────────────────────────────────────────────
' BASIC non ha INFINITY: per "arco assente" usare GRAPH.ABSENT() nei vettori DOUBLE.
' Lo shim normalizza qualunque peso >= 1e29 a INFINITY (sentinella di gmbortools).
PUBLIC FUNCTION ABSENT() AS DOUBLE
    RETURN 1000000000000000000000000000000.0
END FUNCTION

' ── Helper di contenitore (2026-11-06) ──────────────────────────────────────
' La rappresentazione è la matrice: queste tre funzioni evitano i cicli in BASIC
' per le operazioni che la matrice non dà in una chiamata.
' Cammino src→dst ricostruito dai predecessori di DIJKSTRA/BELLMANFORD
' (prev(i) = predecessore, -1 per radice/assente). out() riceve i nodi.
DECLARE NATIVE FUNCTION FINDPATH LIB "gmbgraph" ALIAS "gfx_path_reconstruct__nat" (prev() AS LONG, n AS LONG, src AS LONG, dst AS LONG, out_() AS LONG) AS LONG
' Elenco degli archi (triangolo superiore): eu/ev/ew per ogni arco presente.
DECLARE NATIVE FUNCTION EDGELIST LIB "gmbgraph" ALIAS "gfx_edge_list__nat" (w() AS DOUBLE, n AS LONG, eu() AS LONG, ev() AS LONG, ew() AS DOUBLE) AS LONG
' Vicini di v (riga v della matrice 0/1).
DECLARE NATIVE FUNCTION NEIGHBORLIST LIB "gmbgraph" ALIAS "gfx_neighbors__nat" (adj() AS LONG, n AS LONG, v AS LONG, out_() AS LONG) AS LONG
