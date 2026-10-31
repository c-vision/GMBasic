' GRAPH.* — funzioni qualificate del namespace GRAPH (L5). Costruisce il grafo dal vault
' fixture (samples/Fixtures/graph-vault) scrivendo gli artefatti in work/ accanto a questo
' programma. Il grafo costruito resta il "grafo attivo" della sessione, quindi le funzioni
' GRAPH.* single-brain lo interrogheranno direttamente (nessun registry globale).
' Tutte le funzioni ritornano JSON. I path si derivano da PROJECTFOLDER() (cartella del
' file .bas), mai dalla CWD di lancio.

IMPORT GRAPH

' Fixture condivisa in samples/Fixtures/graph-vault, work/ accanto al programma.
FIXTURE$ = PROJECTFOLDER() + "/../Fixtures/graph-vault"
WORK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

' Costruisce il grafo (8 campi + edge tipizzati) — il nome del brain è il basename del vault.
GRAPH RDD MEMORY
GRAPH BUILD FIXTURE$, WORK$
GRAPH FIND PATH "index.md" TO "trading/momentum.md"
GRAPH UPDATE FIXTURE$, WORK$

' GRAPH.NEIGHBORS(brain, node) — vicini diretti (outgoing+incoming).
PRINT GRAPH.NEIGHBORS("graph-vault", "index.md")

' GRAPH.PATH(brain, from, to) — percorso più corto (BFS).
PRINT GRAPH.PATH("graph-vault", "index.md", "trading/momentum.md")

' GRAPH.PAGERANK(brain [, topK]) — PageRank (power iteration).
PRINT GRAPH.PAGERANK("graph-vault", 3)

' GRAPH.CONNECTED(brain) — componenti connesse.
PRINT GRAPH.CONNECTED("graph-vault")

' GRAPH.STATS(brain) — metriche di base (nodi/edge/dangling + PageRank top).
PRINT GRAPH.STATS("graph-vault")

' GRAPH.STRONGLYCONNECTED(brain) — componenti fortemente connesse (Tarjan).
PRINT GRAPH.STRONGLYCONNECTED("graph-vault")

' GRAPH.TOPOSORT(brain) — ordinamento topologico (Kahn).
PRINT GRAPH.TOPOSORT("graph-vault")

' GRAPH.BIPARTITE(brain) — test di bipartiteness.
PRINT GRAPH.BIPARTITE("graph-vault")

' GRAPH.BETWEENNESS(brain) — centralità betweenness (Brandes).
PRINT GRAPH.BETWEENNESS("graph-vault")

' GRAPH.CLUSTERING(brain, node) — coefficiente di clustering locale.
PRINT GRAPH.CLUSTERING("graph-vault", "index.md")

' GRAPH.DIAMETER(brain) — diametro del grafo (Floyd-Warshall).
PRINT GRAPH.DIAMETER("graph-vault")

' GRAPH.MST(brain) — albero ricoprente minimo (Kruskal).
PRINT GRAPH.MST("graph-vault")

' ── Export estesi (F7 P1): dot / subgraph / JSON by tag/folder / validità ──
' GRAPH.DOT(brain) — export Graphviz (dot).
PRINT GRAPH.DOT("graph-vault")

' GRAPH.SUBGRAPH(brain, root [, depth]) — sottografo a distanza ≤ depth dal nodo (mermaid).
PRINT GRAPH.SUBGRAPH("graph-vault", "index.md", 1)

' GRAPH.BYTAG(brain, tag) — JSON dei nodi con quel tag + i loro edge.
PRINT GRAPH.BYTAG("graph-vault", "strategy")

' GRAPH.BYFOLDER(brain, folder) — JSON dei nodi nella cartella (o sotto) + edge.
PRINT GRAPH.BYFOLDER("graph-vault", "strategies")

' GRAPH.ISVALID(brain) — 1 se il grafo ha almeno un nodo.
PRINT GRAPH.ISVALID("graph-vault")

' ── Algoritmi avanzati (2026-09-07): Dijkstra/HasCycle/Eccentricity/DFS/BFS ──
' GRAPH.DIJKSTRA(brain, from, to) — path a costo minimo (archi pesati).
PRINT GRAPH.DIJKSTRA("graph-vault", "index.md", "trading/momentum.md")

' GRAPH.HASCYCLE(brain) — 1 se il grafo contiene almeno un ciclo.
PRINT GRAPH.HASCYCLE("graph-vault")

' GRAPH.ECCENTRICITY(brain, node) — eccentricità del nodo (distanza massima).
PRINT GRAPH.ECCENTRICITY("graph-vault", "index.md")

' GRAPH.DFS(brain, root) — visita in profondità dal root.
PRINT GRAPH.DFS("graph-vault", "index.md")

' GRAPH.BFS(brain, root) — visita in ampiezza dal root.
PRINT GRAPH.BFS("graph-vault", "index.md")

' GRAPH.QUERYMULTI(query, brains_csv) — merge multi-brain (qui un solo brain).
PRINT GRAPH.QUERYMULTI("momentum", "graph-vault")

PRINT "GraphFunctions ok"
