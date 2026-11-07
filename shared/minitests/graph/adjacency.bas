' MiniTest BRAINS.ADJACENCY — il ponte verso la matematica FFI: nodi (indice = riga/colonna)
' + archi pesati del cervello, così GRAPH/FLOW possono lavorare sul grafo senza duplicare
' gli algoritmi. Seconda riga: cervello assente → errore esplicito.
' EXPECT: {"status":"Success","brain":"graph-vault","n":5,"nodes":["airline/load-factor.md","index.md","strategies/mean-reversion.md","strategies/trend-following.md","trading/momentum.md"],"edges":[[0,4],[1,2],[1,3],[1,4],[2,0],[2,3],[2,4],[3,4],[4,0],[4,3]]}|{"status":"Error","error":"brain \u0027no-such-brain\u0027 non disponibile come graph"}
IMPORT BRAINS

SUB Main()
  FIXTURE$ = PROJECTFOLDER() + "/../../samples/Fixtures/graph-vault"
  WORK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$
  BRAINS RDD MEMORY
  BRAINS BUILD FIXTURE$, WORK$
  PRINT BRAINS.ADJACENCY("graph-vault")
  PRINT BRAINS.ADJACENCY("no-such-brain")
END SUB
