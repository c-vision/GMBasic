' ==============================================================================
' FILE: Collections/CollectionsGraphMissing.bas
' Metodi GRAPH/BAG/HASHRING/RINGBUFFER senza esempio (audit 53):
'   GRAPH: RemoveNode, HasNode, RemoveEdge, GetNeighbors, BFS
'   BAG:   UniqueItems
'   RINGBUFFER: Capacity
' ==============================================================================

IMPORT JSON
PRINT "=== GRAPH: metodi mancanti ==="
DIM MyGraph AS GRAPH<STRING>
MyGraph.AddNode("A") : MyGraph.AddNode("B") : MyGraph.AddNode("C") : MyGraph.AddNode("D")
MyGraph.AddEdge("A", "B", 10) : MyGraph.AddEdge("A", "C", 20)
MyGraph.AddEdge("B", "D", 30) : MyGraph.AddEdge("C", "D", 40)

' HasNode
PRINT "HasNode(A) = "; MyGraph.HasNode("A")
PRINT "HasNode(Z) = "; MyGraph.HasNode("Z")
PRINT

' GetNeighbors
PRINT "GetNeighbors(A) = "; JSON.Stringify(MyGraph.GetNeighbors("A"))
PRINT

' BFS (visita in ampiezza da A)
PRINT "BFS(A) = "; JSON.Stringify(MyGraph.BFS("A"))
PRINT

' DFS (visita in profondità da A)
PRINT "DFS(A) = "; JSON.Stringify(MyGraph.DFS("A"))
PRINT

' RemoveEdge
PRINT "RemoveEdge(A,C) = "; MyGraph.RemoveEdge("A", "C")
PRINT "HasEdge(A,C) dopo remove = "; MyGraph.HasEdge("A", "C")
PRINT

' RemoveNode
PRINT "RemoveNode(D) = "; MyGraph.RemoveNode("D")
PRINT "HasNode(D) dopo remove = "; MyGraph.HasNode("D")
PRINT "NodeCount dopo remove = "; MyGraph.NodeCount()
PRINT

PRINT "=== BAG.UniqueItems ==="
DIM MyBag AS BAG<STRING>
MyBag.Add("x") : MyBag.Add("x") : MyBag.Add("y") : MyBag.Add("z") : MyBag.Add("y")
PRINT "Bag: x, x, y, z, y"
PRINT "UniqueItems = "; JSON.Stringify(MyBag.UniqueItems())
PRINT

PRINT "=== RINGBUFFER.Capacity ==="
DIM MyRing AS RINGBUFFER<LONG>
PRINT "RingBuffer vuota Capacity = "; MyRing.Capacity()
MyRing.ENQUEUE(1) : MyRing.ENQUEUE(2) : MyRing.ENQUEUE(3)
PRINT "Dopo 3 add, Count = "; MyRing.Count(); ", Capacity = "; MyRing.Capacity()

PRINT
PRINT "Completato."
