' GraphLouvainMaxFlow.bas — G4: community detection (Louvain) e flusso massimo (MaxFlow)
' sul namespace GRAPH (grafi wiki/second-brain). Usa la fixture samples/Fixtures/graph-vault.
IMPORT GRAPH

SUB Main()
  FIXTURE$ = PROJECTFOLDER() + "/../../samples/Fixtures/graph-vault"
  WORK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

  GRAPH RDD MEMORY
  GRAPH BUILD FIXTURE$, WORK$

  ' Community detection (Louvain, modularità): count + community per nodo.
  PRINT GRAPH.LOUVAIN("graph-vault")

  ' Flusso massimo (Edmonds-Karp, capacità = pesi degli archi) tra due nodi.
  PRINT GRAPH.MAXFLOW("graph-vault", "index.md", "airline/load-factor.md")
END SUB
