' GRAPH (FFI) — 2ª slice: ponti/articolazione, A*, k-shortest, min-cut, WL, DRLN,
' path cover, isomorfismo, cammini con finestre, BFS direction-optimizing.
' EXPECT: bridges= 3 |art= 2 |astar=3.0: 4 |k= 2 |sw=1.0|wl= 2 |mpc= 1 |iso= 1 |drnl= 1 |bfsopt= 3 :2.0
IMPORT GRAPH

SUB Main()
  ' path non orientato 0-1-2-3
  DIM und(15) AS LONG
  und(1)=1 : und(4)=1
  und(6)=1 : und(9)=1
  und(11)=1 : und(14)=1

  DIM br(15) AS LONG
  PRINT "bridges="; GRAPH.BRIDGES(und, 4, br);

  DIM art(4) AS LONG
  PRINT "|art="; GRAPH.ARTICULATION(und, 4, art);

  ' pesato orientato 0->1->2->3 (peso 1) + 0->3 (peso 10)
  DIM w(15) AS DOUBLE
  DIM INF = GRAPH.ABSENT()
  FOR i = 0 TO 15
    w(i) = INF
  NEXT i
  w(0)=0 : w(1)=1 : w(2)=INF : w(3)=10
  w(4)=INF : w(5)=0 : w(6)=1 : w(7)=INF
  w(8)=INF : w(9)=INF : w(10)=0 : w(11)=1
  w(12)=INF : w(13)=INF : w(14)=INF : w(15)=0

  DIM h(4) AS DOUBLE
  FOR i = 0 TO 3
    h(i) = 0
  NEXT i
  DIM path(4) AS LONG
  DIM info(2) AS LONG
  c = GRAPH.ASTAR(w, 4, h, 0, 3, path, info)
  PRINT "|astar="; FORMAT(c, "#.0"); ":"; info(0);

  ' k-shortest 0->2: 0-1-2 (2) e 0-3-2? non esiste; usa 0-1-2 e 0-2 diretto
  DIM w2(15) AS DOUBLE
  FOR i = 0 TO 15
    w2(i) = INF
  NEXT i
  w2(0)=0 : w2(1)=1 : w2(2)=5
  w2(5)=0 : w2(6)=1
  w2(10)=0
  w2(15)=0
  DIM paths(8) AS LONG
  DIM plens(2) AS LONG
  PRINT "|k="; GRAPH.KSHORTEST(w2, 3, 0, 2, 2, paths, plens);

  DIM swm(15) AS DOUBLE
  FOR i = 0 TO 15
    swm(i) = 0
  NEXT i
  swm(1)=1 : swm(4)=1
  swm(6)=1 : swm(9)=1
  swm(11)=1 : swm(14)=1
  PRINT "|sw="; FORMAT(GRAPH.STOERWAGNER(swm, 4), "#.0");

  ' WL su path 0-1-2
  DIM p3(9) AS LONG
  p3(1)=1 : p3(3)=1 : p3(5)=1 : p3(7)=1
  DIM col(3) AS LONG
  PRINT "|wl="; GRAPH.WL(p3, 3, col);

  ' min path cover su DAG 0->1->2
  DIM dag(9) AS LONG
  dag(1)=1 : dag(5)=1
  PRINT "|mpc="; GRAPH.MINPATHCOVER(dag, 3);

  DIM map(3) AS LONG
  PRINT "|iso="; GRAPH.ISOMORPHIC(p3, 3, p3, 3, map);

  DIM lab(3) AS LONG
  PRINT "|drnl="; GRAPH.DRNL(p3, 3, 0, 1, lab);

  DIM dist(3) AS DOUBLE
  PRINT "|bfsopt="; GRAPH.BFSOPT(p3, 3, 0, 0.5, dist); ":"; FORMAT(dist(2), "#.0");

END SUB
