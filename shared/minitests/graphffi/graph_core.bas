' GRAPH (FFI) — nucleo di teoria dei grafi su matrici dense (matrice 3x3).
' EXPECT: bfs= 3 :[ 0 , 1 , 2 ]|dfs= 3 :[ 0 , 1 , 2 ]|topo= 1 |comp= 1 |scc= 3 |cycle= 0 |bip= 1
' IMPORTA GRAPH (pacchetto gmbgraph); i grafi dei cervelli sono in BRAINS.
IMPORT GRAPH

SUB Main()
  DIM adj(8) AS LONG
  ' 0->1->2 (catena orientata)
  adj(0)=0 : adj(1)=1 : adj(2)=0
  adj(3)=0 : adj(4)=0 : adj(5)=1
  adj(6)=0 : adj(7)=0 : adj(8)=0

  DIM order(3) AS LONG
  c = GRAPH.BFS(adj, 3, 0, order)
  PRINT "bfs="; c; ":[";
  FOR i = 0 TO c-1
    IF i > 0 THEN PRINT ",";
    PRINT order(i);
  NEXT i
  PRINT "]";

  c = GRAPH.DFS(adj, 3, 0, order)
  PRINT "|dfs="; c; ":[";
  FOR i = 0 TO c-1
    IF i > 0 THEN PRINT ",";
    PRINT order(i);
  NEXT i
  PRINT "]";

  PRINT "|topo="; GRAPH.TOPOSORT(adj, 3, order);

  DIM comp(3) AS LONG
  PRINT "|comp="; GRAPH.COMPONENTS(adj, 3, comp);
  PRINT "|scc="; GRAPH.SCC(adj, 3, comp);
  PRINT "|cycle="; GRAPH.HASCYCLE(adj, 3);

  ' catena non orientata (simmetrica) -> bipartita
  DIM und(8) AS LONG
  und(0)=0 : und(1)=1 : und(2)=0
  und(3)=1 : und(4)=0 : und(5)=1
  und(6)=0 : und(7)=1 : und(8)=0
  PRINT "|bip="; GRAPH.ISBIPARTITE(und, 3)
END SUB
