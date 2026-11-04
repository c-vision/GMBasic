' MiniTest BRAINS.QUERYMULTI — merge multi-brain sul grafo costruito dalla fixture.
' EXPECT: {"status":"Success","query":"momentum","brainCount":1,"totalNodes":5,"matches":[{"path":"trading/momentum.md","title":"momentum","brains":["graph-vault"],"neighborCount":5,"neighbors":[{"from":"index.md","alias":null,"direction":"in","brain":"graph-vault"},{"from":"strategies/trend-following.md","alias":null,"direction":"in","brain":"graph-vault"},{"to":"strategies/trend-following.md","alias":null,"direction":"out","brain":"graph-vault"},{"to":"airline/load-factor.md","alias":null,"direction":"out","brain":"graph-vault"},{"from":"airline/load-factor.md","alias":null,"direction":"in","brain":"graph-vault"}]}]}
IMPORT BRAINS
SUB Main()
  FIXTURE$ = PROJECTFOLDER() + "/../../samples/Fixtures/graph-vault"
  WORK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$
  BRAINS BUILD FIXTURE$, WORK$
  PRINT BRAINS.QUERYMULTI("momentum", "graph-vault")
END SUB

