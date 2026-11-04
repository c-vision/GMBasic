' GraphLouvainMaxFlow.bas — G4: community detection (Louvain) e flusso massimo (MaxFlow)
' sul namespace GRAPH (grafi wiki/second-brain). Usa la fixture samples/Fixtures/graph-vault.
IMPORT BRAINS

SUB Main()
  FIXTURE$ = PROJECTFOLDER() + "/../../samples/Fixtures/graph-vault"
  WORK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$

  BRAINS RDD MEMORY
  BRAINS BUILD FIXTURE$, WORK$

  ' Community detection (Louvain, modularità): count + community per nodo.
  PRINT BRAINS.LOUVAIN("graph-vault")

  ' Flusso massimo (Edmonds-Karp, capacità = pesi degli archi) tra due nodi.
  PRINT BRAINS.MAXFLOW("graph-vault", "index.md", "airline/load-factor.md")
END SUB
