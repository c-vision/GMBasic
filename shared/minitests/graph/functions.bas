' MiniTest BRAINS — le 8 funzioni AI/accesso del cervello + i comandi legacy.
' La TEORIA DEI GRAFI non è più qui: BFS/DFS/Dijkstra/PageRank/Louvain/MST/SCC/TopoSort/
' Subgraph/Betweenness/Clustering/Diameter/Eccentricity/HasCycle/Bipartite/Connected/MaxFlow
' sono nel pacchetto FFI GRAPH/FLOW (shared/minitests/graphffi/, samples/graph/GraphFFILab.bas).
' EXPECT: {"status":"Success","brain":"graph-vault","node":"index.md","neighbors":["strategies/mean-reversion.md","strategies/trend-following.md","trading/momentum.md"]}|{"status":"Success","brain":"graph-vault","from":"index.md","to":"trading/momentum.md","path":["index.md","trading/momentum.md"]}|{"status":"Success","brain":"graph-vault","nodes":5,"edges":11,"dangling":0,"pageRankTop":[{"node":"trading/momentum.md","rank":0.45322},{"node":"strategies/trend-following.md","rank":0.247481},{"node":"airline/load-factor.md","rank":0.2308},{"node":"strategies/mean-reversion.md","rank":0.0385},{"node":"index.md","rank":0.03}]}|digraph graph_vault {|rankdir=LR;|"index.md" [label="index"];|"strategies/mean-reversion.md" [label="mean-reversion"];|"strategies/trend-following.md" [label="trend-following"];|"trading/momentum.md" [label="momentum"];|"airline/load-factor.md" [label="load-factor"];|"index.md" -> "strategies/trend-following.md" [label=""];|"index.md" -> "trading/momentum.md" [label=""];|"strategies/trend-following.md" -> "trading/momentum.md" [label=""];|"trading/momentum.md" -> "strategies/trend-following.md" [label=""];|"trading/momentum.md" -> "airline/load-factor.md" [label=""];|"airline/load-factor.md" -> "trading/momentum.md" [label=""];|"strategies/mean-reversion.md" -> "strategies/trend-following.md" [label=""];|"index.md" -> "strategies/mean-reversion.md" [label=""];|"strategies/mean-reversion.md" -> "strategies/trend-following.md" [label=""];|"strategies/mean-reversion.md" -> "trading/momentum.md" [label=""];|"strategies/mean-reversion.md" -> "airline/load-factor.md" [label=""];|}|{|"tag": "strategy",|"nodes": [|{|"id": "strategies/mean-reversion.md",|"title": "mean-reversion"|},|{|"id": "strategies/trend-following.md",|"title": "trend-following"|}|],|"edges": [|{|"from": "strategies/mean-reversion.md",|"to": "strategies/trend-following.md"|},|{|"from": "strategies/mean-reversion.md",|"to": "strategies/trend-following.md"|}|],|"count": 2|}|{|"folder": "strategies",|"nodes": [|{|"id": "strategies/mean-reversion.md",|"title": "mean-reversion"|},|{|"id": "strategies/trend-following.md",|"title": "trend-following"|}|],|"edges": [|{|"from": "strategies/mean-reversion.md",|"to": "strategies/trend-following.md"|},|{|"from": "strategies/mean-reversion.md",|"to": "strategies/trend-following.md"|}|],|"count": 2|}|1|{"status":"Success","query":"momentum","brainCount":1,"totalNodes":5,"matches":[{"path":"trading/momentum.md","title":"momentum","brains":["graph-vault"],"neighborCount":5,"neighbors":[{"from":"index.md","alias":null,"direction":"in","brain":"graph-vault"},{"from":"strategies/trend-following.md","alias":null,"direction":"in","brain":"graph-vault"},{"to":"strategies/trend-following.md","alias":null,"direction":"out","brain":"graph-vault"},{"to":"airline/load-factor.md","alias":null,"direction":"out","brain":"graph-vault"},{"from":"airline/load-factor.md","alias":null,"direction":"in","brain":"graph-vault"}]}]}
IMPORT BRAINS

SUB Main()
  FIXTURE$ = PROJECTFOLDER() + "/../../samples/Fixtures/graph-vault"
  WORK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

  ' ── comandi legacy (REPL/CLI, non BASIC) ──
  BRAINS RDD MEMORY
  BRAINS BUILD FIXTURE$, WORK$
  BRAINS FIND PATH "index.md" TO "trading/momentum.md"
  BRAINS UPDATE FIXTURE$, WORK$

  ' ── accesso al cervello: query, struttura, ispezione, export ──
  PRINT BRAINS.NEIGHBORS("graph-vault", "index.md")
  PRINT BRAINS.PATH("graph-vault", "index.md", "trading/momentum.md")
  PRINT BRAINS.STATS("graph-vault")
  PRINT BRAINS.DOT("graph-vault")
  PRINT BRAINS.BYTAG("graph-vault", "strategy")
  PRINT BRAINS.BYFOLDER("graph-vault", "strategies")
  PRINT BRAINS.ISVALID("graph-vault")
  PRINT BRAINS.QUERYMULTI("momentum", "graph-vault")
END SUB
