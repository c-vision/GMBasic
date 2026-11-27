' MiniTest: operazioni "vuote/assenti" che lanciavano eccezioni CLR — 4 gambe.
' BPLUSTREE.Remove(chiave assente) → 0 (prima IL: NotSupportedException, la
' reflection cercava "Remove" invece di Delete).
' BITREE.Query/QueryRange su albero VUOTO → 0 (prima IL: IndexOutOfRangeException;
' il runtime C e VM/C rendevano 0).
' GRAPH.BFS/DFS con nodo di partenza assente → [] (prima IL: ArgumentException).
' EXPECT: bp.rem= 0  c= 0|bt.q= 0  qr= 0  c= 0|gr.bfs=[] dfs=[]|tri.find=[] c= 0
IMPORT JSON
IMPORT COLLECTIONS
SUB Main()
  DIM bp AS BPLUSTREE<STRING, INTEGER>
  PRINT "bp.rem="; bp.Remove("x"); " c="; bp.Count()
  DIM bt AS BITREE<INTEGER>
  PRINT "bt.q="; bt.Query(0); " qr="; bt.QueryRange(0, 2); " c="; bt.Count()
  DIM gr AS GRAPH<STRING>
  PRINT "gr.bfs="; JSON.STRINGIFY(gr.BFS("x")); " dfs="; JSON.STRINGIFY(gr.DFS("x"))
  DIM tri AS TRIE
  PRINT "tri.find="; JSON.STRINGIFY(tri.FindAll("x")); " c="; tri.Contains("x")
END SUB
