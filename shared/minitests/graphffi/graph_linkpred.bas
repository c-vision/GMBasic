' GRAPH (FFI) — link prediction: baseline su grafo denso (gmbortools gor_linkpred).
' EXPECT: cn= 1 |jac=0.3333|aa=0.9102|ra=0.3333|pa= 4 |katz=0.6667
' IMPORTA GRAPH (pacchetto gmbgraph). Complementare a DRNL/WL (già coperti).
IMPORT GRAPH

SUB Main()
  ' 4 nodi: triangolo 0-1-2 + arco 2-3 (non orientato)
  DIM adj(15) AS LONG
  adj(0)=0  : adj(1)=1  : adj(2)=1  : adj(3)=0
  adj(4)=1  : adj(5)=0  : adj(6)=1  : adj(7)=0
  adj(8)=1  : adj(9)=1  : adj(10)=0 : adj(11)=1
  adj(12)=0 : adj(13)=0 : adj(14)=1 : adj(15)=0

  ' coppia (0,1): un solo vicino in comune (2), grado(2)=3
  PRINT "cn="; GRAPH.LINKCN(adj, 4, 0, 1);
  PRINT "|jac="; FORMAT(GRAPH.LINKJACCARD(adj, 4, 0, 1), "#.0000");
  PRINT "|aa="; FORMAT(GRAPH.LINKADAMICADAR(adj, 4, 0, 1), "#.0000");
  PRINT "|ra="; FORMAT(GRAPH.LINKRESOURCEALLOC(adj, 4, 0, 1), "#.0000");
  PRINT "|pa="; GRAPH.LINKPREFATTACH(adj, 4, 0, 1);

  ' Katz su 2 nodi collegati, beta=0.5 -> beta/(1-beta^2) = 0.6667
  DIM w(3) AS DOUBLE
  w(0)=0.0 : w(1)=1.0 : w(2)=1.0 : w(3)=0.0
  PRINT "|katz="; FORMAT(GRAPH.LINKKATZ(w, 2, 0.5, 0, 1), "#.0000")
END SUB
