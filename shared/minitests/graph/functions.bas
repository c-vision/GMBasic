' MiniTest GRAPH — TUTTE le 23 funzioni + comandi BUILD/UPDATE/FIND PATH/RDD.
' EXPECT: {"status":"Success","brain":"graph-vault","node":"index.md","neighbors":["strategies/mean-reversion.md","strategies/trend-following.md","trading/momentum.md"]}|{"status":"Success","brain":"graph-vault","from":"index.md","to":"trading/momentum.md","path":["index.md","trading/momentum.md"]}|{"status":"Success","brain":"graph-vault","top":[{"node":"trading/momentum.md","rank":0.45322},{"node":"strategies/trend-following.md","rank":0.247481},{"node":"airline/load-factor.md","rank":0.2308}]}|{"status":"Success","brain":"graph-vault","componentCount":1,"components":[["airline/load-factor.md","trading/momentum.md","strategies/mean-reversion.md","strategies/trend-following.md","index.md"]]}|{"status":"Success","brain":"graph-vault","nodes":5,"edges":11,"dangling":0,"pageRankTop":[{"node":"trading/momentum.md","rank":0.45322},{"node":"strategies/trend-following.md","rank":0.247481},{"node":"airline/load-factor.md","rank":0.2308},{"node":"strategies/mean-reversion.md","rank":0.0385},{"node":"index.md","rank":0.03}]}|{"status":"Success","brain":"graph-vault","count":3,"components":[["airline/load-factor.md","trading/momentum.md","strategies/trend-following.md"],["strategies/mean-reversion.md"],["index.md"]]}|{"status":"Error","brain":"graph-vault","error":"il grafo contiene un ciclo"}|{"status":"Success","brain":"graph-vault","bipartite":false}|{"status":"Success","brain":"graph-vault","values":[{"node":"trading/momentum.md","value":0.208333},{"node":"strategies/mean-reversion.md","value":0.041667},{"node":"index.md","value":0},{"node":"strategies/trend-following.md","value":0},{"node":"airline/load-factor.md","value":0}]}|{"status":"Success","brain":"graph-vault","node":"index.md","clustering":1}|{"status":"Success","brain":"graph-vault","diameter":2}|{"status":"Success","brain":"graph-vault","weight":4,"edges":[{"from":"index.md","to":"strategies/trend-following.md","weight":1},{"from":"index.md","to":"trading/momentum.md","weight":1},{"from":"index.md","to":"strategies/mean-reversion.md","weight":1},{"from":"strategies/mean-reversion.md","to":"airline/load-factor.md","weight":1}]}|digraph graph_vault {|rankdir=LR;|"index.md" [label="index"];|"strategies/mean-reversion.md" [label="mean-reversion"];|"strategies/trend-following.md" [label="trend-following"];|"trading/momentum.md" [label="momentum"];|"airline/load-factor.md" [label="load-factor"];|"index.md" -> "strategies/trend-following.md" [label=""];|"index.md" -> "trading/momentum.md" [label=""];|"strategies/trend-following.md" -> "trading/momentum.md" [label=""];|"trading/momentum.md" -> "strategies/trend-following.md" [label=""];|"trading/momentum.md" -> "airline/load-factor.md" [label=""];|"airline/load-factor.md" -> "trading/momentum.md" [label=""];|"strategies/mean-reversion.md" -> "strategies/trend-following.md" [label=""];|"index.md" -> "strategies/mean-reversion.md" [label=""];|"strategies/mean-reversion.md" -> "strategies/trend-following.md" [label=""];|"strategies/mean-reversion.md" -> "trading/momentum.md" [label=""];|"strategies/mean-reversion.md" -> "airline/load-factor.md" [label=""];|}|graph TD|Nindex_md["index"]|Nstrategies_trend_following_md["trend-following"]|Nindex_md --> Nstrategies_trend_following_md|Ntrading_momentum_md["momentum"]|Nindex_md --> Ntrading_momentum_md|Nstrategies_mean_reversion_md["mean-reversion"]|Nindex_md --> Nstrategies_mean_reversion_md|{|"tag": "strategy",|"nodes": [|{|"id": "strategies/mean-reversion.md",|"title": "mean-reversion"|},|{|"id": "strategies/trend-following.md",|"title": "trend-following"|}|],|"edges": [|{|"from": "strategies/mean-reversion.md",|"to": "strategies/trend-following.md"|},|{|"from": "strategies/mean-reversion.md",|"to": "strategies/trend-following.md"|}|],|"count": 2|}|{|"folder": "strategies",|"nodes": [|{|"id": "strategies/mean-reversion.md",|"title": "mean-reversion"|},|{|"id": "strategies/trend-following.md",|"title": "trend-following"|}|],|"edges": [|{|"from": "strategies/mean-reversion.md",|"to": "strategies/trend-following.md"|},|{|"from": "strategies/mean-reversion.md",|"to": "strategies/trend-following.md"|}|],|"count": 2|}|1|1|{"status":"Success","brain":"graph-vault","from":"index.md","to":"trading/momentum.md","path":["index.md","trading/momentum.md"]}|{"status":"Success","brain":"graph-vault","node":"index.md","eccentricity":2}|{"status":"Success","brain":"graph-vault","root":"index.md","order":["index.md","strategies/trend-following.md","trading/momentum.md","airline/load-factor.md","strategies/mean-reversion.md"]}|{"status":"Success","brain":"graph-vault","root":"index.md","order":["index.md","strategies/trend-following.md","trading/momentum.md","strategies/mean-reversion.md","airline/load-factor.md"]}|{"status":"Success","query":"momentum","brainCount":1,"totalNodes":5,"matches":[{"path":"trading/momentum.md","title":"momentum","brains":["graph-vault"],"neighborCount":5,"neighbors":[{"from":"index.md","alias":null,"direction":"in","brain":"graph-vault"},{"from":"strategies/trend-following.md","alias":null,"direction":"in","brain":"graph-vault"},{"to":"strategies/trend-following.md","alias":null,"direction":"out","brain":"graph-vault"},{"to":"airline/load-factor.md","alias":null,"direction":"out","brain":"graph-vault"},{"from":"airline/load-factor.md","alias":null,"direction":"in","brain":"graph-vault"}]}]}
IMPORT GRAPH

SUB Main()
  FIXTURE$ = PROJECTFOLDER() + "/../../samples/Fixtures/graph-vault"
  WORK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

  ' ── comandi legacy ──
  GRAPH RDD MEMORY
  GRAPH BUILD FIXTURE$, WORK$
  GRAPH FIND PATH "index.md" TO "trading/momentum.md"
  GRAPH UPDATE FIXTURE$, WORK$

  ' ── funzioni single-brain ──
  PRINT GRAPH.NEIGHBORS("graph-vault", "index.md")
  PRINT GRAPH.PATH("graph-vault", "index.md", "trading/momentum.md")
  PRINT GRAPH.PAGERANK("graph-vault", 3)
  PRINT GRAPH.CONNECTED("graph-vault")
  PRINT GRAPH.STATS("graph-vault")
  PRINT GRAPH.STRONGLYCONNECTED("graph-vault")
  PRINT GRAPH.TOPOSORT("graph-vault")
  PRINT GRAPH.BIPARTITE("graph-vault")
  PRINT GRAPH.BETWEENNESS("graph-vault")
  PRINT GRAPH.CLUSTERING("graph-vault", "index.md")
  PRINT GRAPH.DIAMETER("graph-vault")
  PRINT GRAPH.MST("graph-vault")
  PRINT GRAPH.DOT("graph-vault")
  PRINT GRAPH.SUBGRAPH("graph-vault", "index.md", 1)
  PRINT GRAPH.BYTAG("graph-vault", "strategy")
  PRINT GRAPH.BYFOLDER("graph-vault", "strategies")
  PRINT GRAPH.ISVALID("graph-vault")
  PRINT GRAPH.HASCYCLE("graph-vault")
  PRINT GRAPH.DIJKSTRA("graph-vault", "index.md", "trading/momentum.md")
  PRINT GRAPH.ECCENTRICITY("graph-vault", "index.md")
  PRINT GRAPH.DFS("graph-vault", "index.md")
  PRINT GRAPH.BFS("graph-vault", "index.md")
  PRINT GRAPH.QUERYMULTI("momentum", "graph-vault")
END SUB

