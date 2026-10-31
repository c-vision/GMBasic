' EXPECT: pesoAB= 10|pesoAX= 0|nodes= 3|grado= 1
IMPORT JSON
SUB Main()
  ' GRAPHOFGETEDGEWEIGHT: peso dell'arco (0 se assente), NodeCount.
  DIM MyGraph AS GRAPH<STRING>
  MyGraph.AddNode("A") : MyGraph.AddNode("B") : MyGraph.AddNode("C")
  MyGraph.AddEdge("A", "B", 10) : MyGraph.AddEdge("A", "C", 20)
  PRINT "pesoAB="; MyGraph.GetEdgeWeight("A", "B")
  PRINT "pesoAX="; MyGraph.GetEdgeWeight("A", "X")
  PRINT "nodes="; MyGraph.NodeCount()
  PRINT "grado="; MyGraph.GetInDegree("B")
END SUB
