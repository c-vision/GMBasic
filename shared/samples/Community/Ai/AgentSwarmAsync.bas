' AgentSwarmAsync.bas — Fase 1 agent-runtime: swarm di N agenti con concorrenza BOUNDED.
' AI.AGENTSWARM(n, prefix, task, maxConcurrency) crea N agenti (prefix0..prefixN-1, se mancano) e li
' esegue ognuno su AgentServer.Ask(task) con al più maxConcurrency in parallelo (SemaphoreSlim cap) —
' NON un thread per agente. Richiede un provider completion attivo (AI SET ACTIVE ... / Ollama/MLX/...);
' senza, il completer fixture deterministico risponde comunque.
IMPORT AI
SUB Main()
  PRINT "swarm-ok"
  ' 4 agenti worker_0..worker_3 rispondono a un task, max 2 in parallelo.
  PRINT AI.AGENTSWARM(4, "worker_", "riassumi in una riga: cos'e' una coda FIFO", 2)
END SUB
