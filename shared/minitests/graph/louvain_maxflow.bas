' EXPECT: {"status":"Success","brain":"graph-vault","count":1,"communities":[{"node":"index.md","community":0},{"node":"strategies/mean-reversion.md","community":0},{"node":"strategies/trend-following.md","community":0},{"node":"trading/momentum.md","community":0},{"node":"airline/load-factor.md","community":0}]}|{"status":"Success","brain":"graph-vault","from":"index.md","to":"airline/load-factor.md","flow":2}
' MiniTest BRAINS.LOUVAIN / BRAINS.MAXFLOW — G4, su brain reale (fixture graph-vault).
IMPORT BRAINS

SUB Main()
  FIXTURE$ = PROJECTFOLDER() + "/../../samples/Fixtures/graph-vault"
  WORK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$
  BRAINS RDD MEMORY
  BRAINS BUILD FIXTURE$, WORK$
  PRINT BRAINS.LOUVAIN("graph-vault")
  PRINT BRAINS.MAXFLOW("graph-vault", "index.md", "airline/load-factor.md")
END SUB
