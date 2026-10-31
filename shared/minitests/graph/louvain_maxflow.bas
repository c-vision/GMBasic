' EXPECT: {"status":"Success","brain":"graph-vault","count":1,"communities":[{"node":"index.md","community":0},{"node":"strategies/mean-reversion.md","community":0},{"node":"strategies/trend-following.md","community":0},{"node":"trading/momentum.md","community":0},{"node":"airline/load-factor.md","community":0}]}|{"status":"Success","brain":"graph-vault","from":"index.md","to":"airline/load-factor.md","flow":2}
' MiniTest GRAPH.LOUVAIN / GRAPH.MAXFLOW — G4, su brain reale (fixture graph-vault).
IMPORT GRAPH

SUB Main()
  FIXTURE$ = PROJECTFOLDER() + "/../../samples/Fixtures/graph-vault"
  WORK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$
  GRAPH RDD MEMORY
  GRAPH BUILD FIXTURE$, WORK$
  PRINT GRAPH.LOUVAIN("graph-vault")
  PRINT GRAPH.MAXFLOW("graph-vault", "index.md", "airline/load-factor.md")
END SUB
