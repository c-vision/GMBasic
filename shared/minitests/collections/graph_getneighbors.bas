' GRAPH.GetNeighbors(node): lista di record {node, weight} (parità 4 backend col runtime
' gmb_graph_neighbors_val). Prima: IL dict, PCode lista di dict vuoti.
' JSON.Stringify diretto (la forma DIM x = <list> ha tipi diversi fra i backend).
' EXPECT: m=[{"node":"B","weight":10},{"node":"C","weight":20}]|isola=[]
IMPORT COLLECTIONS
IMPORT JSON
SUB Main()
  DIM g AS GRAPH<STRING>
  g.AddNode("A") : g.AddNode("B") : g.AddNode("C")
  g.AddEdge("A", "B", 10)
  g.AddEdge("A", "C", 20)
  PRINT "m="; JSON.Stringify(g.GetNeighbors("A"))
  PRINT "isola="; JSON.Stringify(g.GetNeighbors("C"))
END SUB
