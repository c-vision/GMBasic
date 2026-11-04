' GRAPH (FFI flow) — max-flow/min-cut/Dinic, matching, min-cost, Gomory-Hu, multicommodity.
' EXPECT: maxflow=5.0|mincut=5.0|dinic=5.0|mcf=4.0:2.0|bip= 3 |gen= 1 |wmatch=1.0|gh= 3 |multi=2.0|circ= 1 
IMPORT GRAPH

SUB Main()
  ' rete 4 nodi: 0->1(3), 0->2(2), 1->2(1), 1->3(2), 2->3(3); s=0 t=3
  DIM cap(15) AS DOUBLE
  cap(1)=3 : cap(2)=2
  cap(6)=1 : cap(7)=2
  cap(11)=3
  PRINT "maxflow="; FORMAT(GRAPH.MAXFLOW(cap, 4, 0, 3), "#.0");

  DIM side(4) AS LONG
  PRINT "|mincut="; FORMAT(GRAPH.MINCUT(cap, 4, 0, 3, side), "#.0");
  PRINT "|dinic="; FORMAT(GRAPH.DINIC(cap, 4, 0, 3), "#.0");

  ' min-cost flow: 0->1 (cap 2 cost 1), 1->2 (cap 2 cost 1)
  DIM mc(8) AS DOUBLE
  DIM cost(8) AS DOUBLE
  mc(1)=2 : mc(5)=2
  cost(1)=1 : cost(5)=1
  DIM fv(1) AS DOUBLE
  PRINT "|mcf="; FORMAT(GRAPH.MINCOSTFLOW(mc, cost, 3, 0, 2, fv), "#.0"); ":"; FORMAT(fv(0), "#.0");

  ' matching bipartito 3x3 identità
  DIM bm(8) AS LONG
  bm(0)=1 : bm(4)=1 : bm(8)=1
  DIM ml(3) AS LONG
  DIM mr(3) AS LONG
  PRINT "|bip="; GRAPH.BIPARTITEMATCH(bm, 3, 3, ml, mr);

  ' matching generale: triangolo -> cardinalità 1
  DIM tri(8) AS LONG
  tri(1)=1 : tri(3)=1 : tri(5)=1 : tri(7)=1
  DIM gm(3) AS LONG
  PRINT "|gen="; GRAPH.GENERALMATCH(tri, 3, gm);

  ' matching pesato sul triangolo (pesi 1)
  DIM wm(8) AS DOUBLE
  wm(1)=1 : wm(3)=1 : wm(5)=1 : wm(7)=1
  DIM wmatch(3) AS LONG
  PRINT "|wmatch="; FORMAT(GRAPH.WEIGHTEDMATCH(wm, 3, wmatch), "#.0");

  ' Gomory-Hu su path non orientato 0-1-2-3 (pesi 1)
  DIM gh(15) AS DOUBLE
  gh(1)=1 : gh(4)=1 : gh(6)=1 : gh(9)=1 : gh(11)=1 : gh(14)=1
  DIM parent(4) AS LONG
  DIM weight(4) AS DOUBLE
  PRINT "|gh="; GRAPH.GOMORYHU(gh, 4, parent, weight);

  ' multicommodity: una commodity 0->2 domanda 2
  DIM comm(3) AS DOUBLE
  comm(0)=0 : comm(1)=2 : comm(2)=2
  DIM cflow(8) AS DOUBLE
  DIM mc2(8) AS DOUBLE
  mc2(1)=2 : mc2(5)=2
  PRINT "|multi="; FORMAT(GRAPH.MULTICOMMODITY(mc2, 3, 1, comm, cflow), "#.0");

  ' circolazione banale (lower=0, upper=1 su 0->1, domanda netta 0)
  DIM lo(8) AS DOUBLE
  DIM up(8) AS DOUBLE
  DIM dem(3) AS DOUBLE
  DIM cfl(8) AS DOUBLE
  up(1)=1
  PRINT "|circ="; GRAPH.CIRCULATION(lo, up, 3, dem, cfl)
  PRINT ""
END SUB
