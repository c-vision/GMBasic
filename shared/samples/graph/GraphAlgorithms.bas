' GRAPH.DIJKSTRA / GRAPH.HASCYCLE / GRAPH.ECCENTRICITY / GRAPH.DFS / GRAPH.BFS —
' algoritmi avanzati del knowledge graph (2026-09-07). Costruisce il grafo dal vault fixture
' (samples/Fixtures/graph-vault) e interroga i 5 algoritmi. Tutte ritornano JSON (o "1"/"0"
' per i predicati). Path da PROJECTFOLDER(), artefatti in work/ accanto al programma.

IMPORT GRAPH

FIXTURE$ = PROJECTFOLDER() + "/../Fixtures/graph-vault"
WORK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

' Costruisce il grafo — il nome del brain è il basename del vault ("graph-vault").
GRAPH BUILD FIXTURE$, WORK$

' GRAPH.HASCYCLE(brain) — 1 se il grafo contiene almeno un ciclo. La fixture ha link
' reciproci (momentum<->trend-following, momentum<->load-factor) quindi deve dare 1.
PRINT "hasCycle="; GRAPH.HASCYCLE("graph-vault")

' GRAPH.DIJKSTRA(brain, from, to) — path a costo minimo (archi pesati, qui tutti peso 1).
' Dalla radice index.md a mean-reversion passando per le strategie.
PRINT "dijkstra="; GRAPH.DIJKSTRA("graph-vault", "index.md", "strategies/mean-reversion.md")

' GRAPH.BFS(brain, root) — visita in ampiezza da index.md (ordine per livelli).
PRINT "bfs="; GRAPH.BFS("graph-vault", "index.md")

' GRAPH.DFS(brain, root) — visita in profondità da index.md.
PRINT "dfs="; GRAPH.DFS("graph-vault", "index.md")

' GRAPH.ECCENTRICITY(brain, node) — eccentricità di index.md (distanza massima verso
' qualunque altro nodo; nella fixture il path più lungo parte da index.md).
PRINT "ecc="; GRAPH.ECCENTRICITY("graph-vault", "index.md")

PRINT "GraphAlgorithms ok"
