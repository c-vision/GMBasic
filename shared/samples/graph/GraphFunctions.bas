' GRAPH.* — funzioni qualificate del namespace GRAPH (L5). Costruisce il grafo dal vault
' fixture (samples/Fixtures/graph-vault) scrivendo gli artefatti in work/ accanto a questo
' programma. Il grafo costruito resta il "grafo attivo" della sessione, quindi le funzioni
' GRAPH.* single-brain lo interrogheranno direttamente (nessun registry globale).
' Tutte le funzioni ritornano JSON. I path si derivano da PROJECTFOLDER() (cartella del
' file .bas), mai dalla CWD di lancio.

IMPORT BRAINS

IMPORT AI
' Fixture condivisa in samples/Fixtures/graph-vault, work/ accanto al programma.
FIXTURE$ = PROJECTFOLDER() + "/../Fixtures/graph-vault"
WORK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

' Costruisce il grafo (8 campi + edge tipizzati) — il nome del brain è il basename del vault.
BRAINS RDD MEMORY
BRAINS BUILD FIXTURE$, WORK$
BRAINS FIND PATH "index.md" TO "trading/momentum.md"
BRAINS UPDATE FIXTURE$, WORK$

' BRAINS.NEIGHBORS(brain, node) — vicini diretti (outgoing+incoming).
PRINT BRAINS.NEIGHBORS("graph-vault", "index.md")

' BRAINS.PATH(brain, from, to) — percorso più corto (BFS).
PRINT BRAINS.PATH("graph-vault", "index.md", "trading/momentum.md")

' BRAINS.PAGERANK(brain [, topK]) — PageRank (power iteration).
PRINT BRAINS.PAGERANK("graph-vault", 3)

' BRAINS.CONNECTED(brain) — componenti connesse.
PRINT BRAINS.CONNECTED("graph-vault")

' BRAINS.STATS(brain) — metriche di base (nodi/edge/dangling + PageRank top).
PRINT BRAINS.STATS("graph-vault")

' BRAINS.STRONGLYCONNECTED(brain) — componenti fortemente connesse (Tarjan).
PRINT BRAINS.STRONGLYCONNECTED("graph-vault")

' BRAINS.TOPOSORT(brain) — ordinamento topologico (Kahn).
PRINT BRAINS.TOPOSORT("graph-vault")

' BRAINS.BIPARTITE(brain) — test di bipartiteness.
PRINT BRAINS.BIPARTITE("graph-vault")

' BRAINS.BETWEENNESS(brain) — centralità betweenness (Brandes).
PRINT BRAINS.BETWEENNESS("graph-vault")

' BRAINS.CLUSTERING(brain, node) — coefficiente di clustering locale.
PRINT BRAINS.CLUSTERING("graph-vault", "index.md")

' BRAINS.DIAMETER(brain) — diametro del grafo (Floyd-Warshall).
PRINT BRAINS.DIAMETER("graph-vault")

' BRAINS.MST(brain) — albero ricoprente minimo (Kruskal).
PRINT BRAINS.MST("graph-vault")

' ── Export estesi (F7 P1): dot / subgraph / JSON by tag/folder / validità ──
' BRAINS.DOT(brain) — export Graphviz (dot).
PRINT BRAINS.DOT("graph-vault")

' BRAINS.SUBGRAPH(brain, root [, depth]) — sottografo a distanza ≤ depth dal nodo (mermaid).
PRINT BRAINS.SUBGRAPH("graph-vault", "index.md", 1)

' AI.GRAPHTAG(brain, tag) — JSON dei nodi con quel tag + i loro edge.
PRINT AI.GRAPHTAG("graph-vault", "strategy")

' AI.GRAPHFOLDER(brain, folder) — JSON dei nodi nella cartella (o sotto) + edge.
PRINT AI.GRAPHFOLDER("graph-vault", "strategies")

' BRAINS.ISVALID(brain) — 1 se il grafo ha almeno un nodo.
PRINT BRAINS.ISVALID("graph-vault")

' ── Algoritmi avanzati (2026-09-07): Dijkstra/HasCycle/Eccentricity/DFS/BFS ──
' BRAINS.DIJKSTRA(brain, from, to) — path a costo minimo (archi pesati).
PRINT BRAINS.DIJKSTRA("graph-vault", "index.md", "trading/momentum.md")

' BRAINS.HASCYCLE(brain) — 1 se il grafo contiene almeno un ciclo.
PRINT BRAINS.HASCYCLE("graph-vault")

' BRAINS.ECCENTRICITY(brain, node) — eccentricità del nodo (distanza massima).
PRINT BRAINS.ECCENTRICITY("graph-vault", "index.md")

' BRAINS.DFS(brain, root) — visita in profondità dal root.
PRINT BRAINS.DFS("graph-vault", "index.md")

' BRAINS.BFS(brain, root) — visita in ampiezza dal root.
PRINT BRAINS.BFS("graph-vault", "index.md")

' AI.GRAPHMULTI(query, brains_csv) — merge multi-brain (qui un solo brain).
PRINT AI.GRAPHMULTI("momentum", "graph-vault")

PRINT "GraphFunctions ok"
