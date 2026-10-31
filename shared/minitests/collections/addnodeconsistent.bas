' EXPECT: -1|-1|0|0|0
IMPORT COLLECTIONS
SUB Main()
  ' GRAPH AddNodeConsistent: aggiunge il nodo se assente e ritorna -1 (vero) se
  ' il nodo ESISTE dopo la chiamata (nuovo o gia' presente); 0 su errore.
  ' Parita' 4 backend: helper condiviso gmb_graph_add_node_consistent (runtime C).
  DIM g AS GRAPH<STRING>
  PRINT g.AddNodeConsistent("A")
  PRINT g.AddNodeConsistent("A")
  DIM p AS GRAPH<STRING>
  p.AddNode("x")
  p.AddNode("y")
  p.AddEdge("x", "y", 1)
  PRINT p.GetInDegree("x")
  DIM q AS GRAPH<STRING>
  PRINT q.HasEdge("a", "b")
  DIM e AS GRAPH<STRING>
  e.AddNode("m")
  e.AddNode("n")
  PRINT e.GetEdgeWeight("m", "n")
END SUB
