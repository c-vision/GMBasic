' AI.AGENTRUNGRAPH — l'agente esplora il knowledge graph del Second Brain (2026-09-07).
' Il loop agente usa i tool graph.* (query/neighbors/path/dijkstra/bfs/dfs/hubs/stats) per
' rispondere a una domanda che richiede di NAVIGARE le relazioni tra note — non la ricerca
' vettoriale, ma la struttura del grafo. Il dispatch dei tool è nativo C# verso
' GraphMultiQuery (sola lettura; nessun codice BASIC nel GPM).
'
' NOTA: richiede un provider di completamento che faccia TOOL-CALLING (es. un modello locale
' MLX/llama o un provider remoto). Con la fixture-completion il loop non produce tool_calls e
' risponde senza esplorare il grafo — il sample COMPILA comunque (Conformance) e va eseguito
' con un modello reale per vedere l'esplorazione.
IMPORT GRAPH
IMPORT AI WITH ALIAS

SUB Main()
  ' 1) costruisci il grafo (fixture condivisa), brain = "graph-vault"
  FIXTURE$ = PROJECTFOLDER() + "/../Fixtures/graph-vault"
  WORK$ = PROJECTFOLDER() + "/work"
  IF IO.DirExists(WORK$) = 0 THEN MKDIR WORK$
  BRAINS BUILD FIXTURE$, WORK$

  ' 2) agente
  AI AGENT CREATE "graph-agent"

  ' 3) loop agente sul GRAFO: il modello naviga i tool graph.* per rispondere
  DIM answer$ AS STRING
  answer$ = AI.AGENTRUNGRAPH("graph-agent", _
    "Quali strategie sono collegate a trading/momentum.md nel grafo graph-vault? " + _
    "Usa graph.neighbors per esplorare e poi riassumi i collegamenti.", 5)
  PRINT "=== RISPOSTA ==="
  PRINT answer$

  AI AGENT DELETE "graph-agent"
  PRINT "done"
END SUB
