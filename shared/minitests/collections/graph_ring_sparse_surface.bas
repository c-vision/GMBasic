' MiniTest: superficie canonica di GRAPH / HASHRING / RINGBUFFER / SPARSESET / BITREE /
' BPLUSTREE sulle 4 gambe.
' GRAPH: AddNode/AddEdge, NodeCount/HasNode/HasEdge/GetEdgeWeight/GetInDegree, BFS/DFS,
'   Dijkstra=[{node,dist}] ordinata, GetNeighbors=[{node,weight}], RemoveEdge/RemoveNode -1/0.
' HASHRING (ConsistentHash): GetNode(key) deterministico, AddNode/RemoveNode.
' RINGBUFFER (CircularQueue): Capacity/Count/Peek/ToArray; Enqueue a buffer pieno sovrascrive
'   il più vecchio; Dequeue dal più vecchio.
' SPARSESET: Count/Capacity (= 2° argomento del costruttore, CommonLib _dense.Length),
'   HasItem -1/0, Remove/Clear.
' BITREE: Count = numero di valori sorgente, Query/QueryRange in indici 0-based, Update(delta).
' BPLUSTREE: Count/Min/Max ({chiave,valore})/Get/Contains/Keys/ToArray/JSON/ExtractMax, Remove.
' Divergenze corrette il 2026-11-26: PCode BPlusTree.Remove era nil e non eliminava (la
' riflessione cercava "Remove" invece di "Delete"); VM/C BITree.Count era 0 (il letterale
' passava da Update senza impostare _size); SPARSESET.Capacity era 8 su VM/C e 0 su C99
' (capacity del costruttore mai applicata); il parser C scartava gli argomenti del
' costruttore oltre il primo (NEW SPARSESET(max, capacity)).
' EXPECT: g.nodecount= 3|g.hasnode=-1|g.hasnode.no= 0|g.hasedge=-1|g.weight= 10|g.indeg= 2|g.bfs=["A","B","C"]|g.dfs=["A","B","C"]|g.dijkstra=[{"node":"A","dist":0},{"node":"B","dist":10},{"node":"C","dist":15}]|g.neighbors=[{"node":"B","weight":10},{"node":"C","weight":20}]|g.remedge=-1|g.remnode=-1|g.nodecount2= 2|r.node1=s3|r.node2=s3|r.node3=s3|rb.capacity= 3|rb.count= 3|rb.peek=A|rb.full=["A","B","C"]|rb.overflow=["B","C","D"]|rb.deq=B|rb.after=["C","D"]|rb.count2= 2|ss.count= 3|ss.capacity= 10|ss.has=-1|ss.has.no=-1|ss.removed= 0|ss.count2= 2|ss.cleared= 0|bit.count= 5|bit.q2= 6|bit.qr13= 9|bit.qr13b= 19|bp.count= 3|bp.min=["alpha",10]|bp.max=["zulu",30]|bp.get= 20|bp.has=-1|bp.keys=["alpha","bravo","zulu"]|bp.toarr=[{"Key":"alpha","Value":10},{"Key":"bravo","Value":20},{"Key":"zulu","Value":30}]|bp.json={"alpha":10,"bravo":20,"zulu":30}|bp.exmax=["zulu",30]|bp.max2=["bravo",20]|bp.rem=-1|bp.count2= 1
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM g AS GRAPH<STRING>
  g.AddNode("A")
  g.AddNode("B")
  g.AddNode("C")
  g.AddEdge("A", "B", 10)
  g.AddEdge("A", "C", 20)
  g.AddEdge("B", "C", 5)
  PRINT "g.nodecount="; g.NodeCount()
  PRINT "g.hasnode="; g.HasNode("C")
  PRINT "g.hasnode.no="; g.HasNode("Z")
  PRINT "g.hasedge="; g.HasEdge("A", "C")
  PRINT "g.weight="; g.GetEdgeWeight("A", "B")
  PRINT "g.indeg="; g.GetInDegree("C")
  PRINT "g.bfs="; JSON.STRINGIFY(g.BFS("A"))
  PRINT "g.dfs="; JSON.STRINGIFY(g.DFS("A"))
  PRINT "g.dijkstra="; JSON.STRINGIFY(g.Dijkstra("A"))
  PRINT "g.neighbors="; JSON.STRINGIFY(g.GetNeighbors("A"))
  PRINT "g.remedge="; g.RemoveEdge("A", "B")
  PRINT "g.remnode="; g.RemoveNode("C")
  PRINT "g.nodecount2="; g.NodeCount()

  DIM servers = ["s1", "s2", "s3"]
  DIM r AS HASHRING<STRING> = NEW HASHRING<STRING>(servers)
  PRINT "r.node1="; r.GetNode("user:123")
  r.AddNode("s4")
  PRINT "r.node2="; r.GetNode("user:123")
  r.RemoveNode("s4")
  PRINT "r.node3="; r.GetNode("user:123")

  DIM rb AS RINGBUFFER<STRING> = NEW RINGBUFFER<STRING>(3)
  PRINT "rb.capacity="; rb.Capacity()
  rb.Enqueue("A") : rb.Enqueue("B") : rb.Enqueue("C")
  PRINT "rb.count="; rb.Count()
  PRINT "rb.peek="; rb.Peek()
  PRINT "rb.full="; JSON.STRINGIFY(rb.ToArray())
  rb.Enqueue("D")
  PRINT "rb.overflow="; JSON.STRINGIFY(rb.ToArray())
  PRINT "rb.deq="; rb.Dequeue()
  PRINT "rb.after="; JSON.STRINGIFY(rb.ToArray())
  PRINT "rb.count2="; rb.Count()

  DIM ss AS SPARSESET = NEW SPARSESET(1000, 10)
  ss.Add(100) : ss.Add(500) : ss.Add(999)
  PRINT "ss.count="; ss.Count()
  PRINT "ss.capacity="; ss.Capacity()
  PRINT "ss.has="; ss.HasItem(500)
  PRINT "ss.has.no="; ss.HasItem(100)
  ss.Remove(100)
  PRINT "ss.removed="; ss.HasItem(100)
  PRINT "ss.count2="; ss.Count()
  ss.Clear()
  PRINT "ss.cleared="; ss.Count()

  DIM bit AS BITREE<INTEGER> = NEW BITREE<INTEGER>([1, 2, 3, 4, 5])
  PRINT "bit.count="; bit.Count()
  PRINT "bit.q2="; bit.Query(2)
  PRINT "bit.qr13="; bit.QueryRange(1, 3)
  bit.Update(1, 10)
  PRINT "bit.qr13b="; bit.QueryRange(1, 3)

  DIM bp AS BPLUSTREE<STRING, INTEGER> = NEW BPLUSTREE<STRING, INTEGER>(3)
  bp.Add("zulu", 30) : bp.Add("alpha", 10) : bp.Add("bravo", 20)
  PRINT "bp.count="; bp.Count()
  PRINT "bp.min="; JSON.STRINGIFY(bp.Min())
  PRINT "bp.max="; JSON.STRINGIFY(bp.Max())
  PRINT "bp.get="; bp.Get("bravo")
  PRINT "bp.has="; bp.Contains("alpha")
  PRINT "bp.keys="; JSON.STRINGIFY(bp.Keys())
  PRINT "bp.toarr="; JSON.STRINGIFY(bp.ToArray())
  PRINT "bp.json="; bp.ToJson()
  PRINT "bp.exmax="; JSON.STRINGIFY(bp.ExtractMax())
  PRINT "bp.max2="; JSON.STRINGIFY(bp.Max())
  PRINT "bp.rem="; bp.Remove("bravo")
  PRINT "bp.count2="; bp.Count()
END SUB
