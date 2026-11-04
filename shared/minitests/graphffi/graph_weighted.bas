' GRAPH (FFI) — percorsi pesati, centralità, MST, colorazione/clique/chiusura.
' EXPECT: dij= 1 :3.0: 1 |fw= 1 :3.0|bf= 1 :2.0: 3 :1.0|diam=3.0|ecc0=3.0|mst= 2 :3.0|clu=0.0|col= 2 |clq= 2 |tc= 1 : 1 |sub= 0 :-1 |pr= 1 :1.000|btw=-1 |clo= 1 :-1 |lv= 1 
IMPORT GRAPH

SUB Main()
  ' 3 nodi: 0-1 (1), 1-2 (2), 0-2 assente
  INF = GRAPH.ABSENT()
  DIM w(8) AS DOUBLE
  w(0)=0 : w(1)=1 : w(2)=INF
  w(3)=1 : w(4)=0 : w(5)=2
  w(6)=INF : w(7)=2 : w(8)=0

  DIM dist(3) AS DOUBLE
  DIM prev(3) AS LONG
  r = GRAPH.DIJKSTRA(w, 3, 0, dist, prev)
  PRINT "dij="; r; ":"; FORMAT(dist(2), "#.0"); ":"; prev(2);

  DIM fw(8) AS DOUBLE
  r = GRAPH.FLOYDWARSHALL(w, 3, fw)
  PRINT "|fw="; r; ":"; FORMAT(fw(2), "#.0");

  ' Bellman-Ford con un arco NEGATIVO (1->3 = -3), nessun ciclo negativo.
  ' Grafo: 0->1=4 · 1->2=5 · 1->3=-3 · 2->3=2 · 3->2=1  =>  dist = 0, 4, 2 (via 3), 1
  DIM nb(15) AS DOUBLE
  FOR i = 0 TO 15 : nb(i) = INF : NEXT i
  nb(0)=0 : nb(5)=0 : nb(10)=0 : nb(15)=0
  nb(1)=4 : nb(6)=5 : nb(7)=-3 : nb(11)=2 : nb(14)=1
  DIM bd(3) AS DOUBLE
  DIM bp(3) AS LONG
  r = GRAPH.BELLMANFORD(nb, 4, 0, bd, bp)
  PRINT "|bf="; r; ":"; FORMAT(bd(2), "#.0"); ":"; bp(2); ":"; FORMAT(bd(3), "#.0");

  PRINT "|diam="; FORMAT(GRAPH.DIAMETER(w, 3), "#.0");
  PRINT "|ecc0="; FORMAT(GRAPH.ECCENTRICITY(w, 3, 0), "#.0");

  DIM eu(2) AS LONG
  DIM ev(2) AS LONG
  DIM ew(2) AS DOUBLE
  m = GRAPH.MSTKRUSKAL(w, 3, eu, ev, ew)
  PRINT "|mst="; m; ":"; FORMAT(ew(0)+ew(1), "#.0");

  ' catena non orientata per clustering/colorazione/clique
  DIM und(8) AS LONG
  und(0)=0 : und(1)=1 : und(2)=0
  und(3)=1 : und(4)=0 : und(5)=1
  und(6)=0 : und(7)=1 : und(8)=0
  PRINT "|clu="; FORMAT(GRAPH.CLUSTERING(und, 3, 1), "#.0");

  DIM colors(3) AS LONG
  PRINT "|col="; GRAPH.COLORING(und, 3, colors);

  DIM clique(3) AS LONG
  PRINT "|clq="; GRAPH.MAXCLIQUE(und, 3, clique);

  DIM closure(8) AS LONG
  r = GRAPH.TRANSITIVECLOSURE(und, 3, closure)
  PRINT "|tc="; r; ":"; closure(2);

  DIM nodes(2) AS LONG
  nodes(0)=0 : nodes(1)=2
  DIM sg(4) AS DOUBLE
  r = GRAPH.SUBGRAPH(w, 3, nodes, 2, sg)
  PRINT "|sub="; r; ":"; (sg(1) > 100000000000000000000000000000.0);

  DIM rank(3) AS DOUBLE
  r = GRAPH.PAGERANK(w, 3, rank, 0.85, 50)
  PRINT "|pr="; r; ":"; FORMAT(rank(0)+rank(1)+rank(2), "#.000");

  DIM bc(3) AS DOUBLE
  b = GRAPH.BETWEENNESS(w, 3, bc)
  PRINT "|btw="; (b > 0);

  DIM cc(3) AS DOUBLE
  r = GRAPH.CLOSENESS(und, 3, cc)
  PRINT "|clo="; r; ":"; (cc(1) > 0);

  DIM comm(3) AS LONG
  r = GRAPH.LOUVAIN(w, 3, comm, 1.0)
  PRINT "|lv="; r
END SUB
