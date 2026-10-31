' B4 — AI.AGENTSWARM con concorrenza BOUNDED (maxConcurrency).
' Crea N agenti (prefix0..N-1), li interroga in parallelo con concorrenza <= maxConcurrency,
' risultati per indice (ordine preservato). Runtime condiviso gmb_agent_swarm (C) / AgentBatch (C#).
IMPORT AI

SUB Main
  DIM s AS STRING
  s = AI.AGENTSWARM(3, "sw", "Spiega cosa e' un DAG in una riga.", 2)
  IF LEN(s) > 0 THEN PRINT "swarm-ok" ELSE PRINT "swarm-FAIL"
END SUB
