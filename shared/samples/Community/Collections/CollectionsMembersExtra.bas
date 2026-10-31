' ==============================================================================
' FILE: Collections/CollectionsMembersExtra.bas
' Membri di collection avanzati (F1 Incongruenze — parita' 4 backend):
'   LIST: AddRange, AddUnique, AddAfter, AddBefore, IsEmpty, Front, Back
'   QUEUE: Front, Back, IsEmpty
'   DICT: Set, SetKeyValue, Get, Keys, MaxPair, MinPair
'   GRAPH: AddNode, AddEdge, GetEdgeWeight, GetInDegree, NodeCount
' Le grafe del membro sono CASE-INSENSITIVE (.set()/.Set()/.SET() sono lo stesso
' metodo: il dispatch usa il TokenKind, il lexeme e' solo grafia).
' ==============================================================================

IMPORT COLLECTIONS
IMPORT JSON
PRINT "=== LIST: AddRange / AddUnique / AddAfter / AddBefore ==="
DIM a AS LIST OF STRING = NEW LIST OF STRING
a.Add("1") : a.Add("2")
DIM b AS LIST OF STRING = NEW LIST OF STRING
b.Add("3") : b.Add("4")
a.AddRange(b)
PRINT "AfterRange: "; JSON.Stringify(a.ToArray())
PRINT "Count: "; a.Count()

DIM u AS LIST OF STRING = NEW LIST OF STRING
u.AddUnique("x") : u.AddUnique("y") : u.AddUnique("x")
PRINT "UniqueCount: "; u.UniqueCount()

DIM ins AS LIST OF STRING = NEW LIST OF STRING
ins.Add("1") : ins.Add("2") : ins.Add("3")
ins.AddAfter(0, "2b") : ins.AddBefore(0, "0")
PRINT "Range: "; JSON.Stringify(ins.ToArray())

PRINT
PRINT "=== LIST: IsEmpty / Front / Back ==="
DIM v AS LIST OF STRING = NEW LIST OF STRING
PRINT "Vuota: "; v.IsEmpty()
v.Add("alfa") : v.Add("beta")
PRINT "Front: "; v.Front()
PRINT "Back: "; v.Back()
PRINT "VuotaDopo: "; v.IsEmpty()

PRINT
PRINT "=== QUEUE: Enqueue / Front / Back / Dequeue ==="
DIM q AS QUEUE OF STRING = NEW QUEUE OF STRING
q.Enqueue("1") : q.Enqueue("2")
PRINT "Front: "; q.Front()
PRINT "Back: "; q.Back()
PRINT "Dequeue: "; q.Dequeue()
PRINT "Vuota: "; q.IsEmpty()

PRINT "=== DICT: Set / SetKeyValue / Get / MaxPair / MinPair ==="
DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
d.set("a", 1) : d.add("b", 3) : d.add("c", 2)
PRINT "GetLow: "; d.get("a")
d.SetKeyValue("d", 9)
PRINT "GetD: "; d.Get("d")
PRINT "MaxPair: "; d.MaxPair()
PRINT "MinPair: "; d.MinPair()
PRINT "Keys: "; JSON.Stringify(d.Keys())

PRINT
PRINT "=== GRAPH: AddNode / AddEdge / GetEdgeWeight / GetInDegree ==="
DIM MyGraph AS GRAPH<STRING>
MyGraph.AddNode("A") : MyGraph.AddNode("B") : MyGraph.AddNode("C")
MyGraph.AddEdge("A", "B", 10) : MyGraph.AddEdge("A", "C", 20)
PRINT "PesoAB: "; MyGraph.GetEdgeWeight("A", "B")
PRINT "PesoAssente: "; MyGraph.GetEdgeWeight("A", "X")
PRINT "Nodes: "; MyGraph.NodeCount()
PRINT "GradoB: "; MyGraph.GetInDegree("B")
PRINT "=== FINE ==="
