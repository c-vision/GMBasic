' AI.REPLAYSTART / REPLAYRECORD / REPLAYSTOP / REPLAYGET / REPLAYLIST / REPLAYCLEAR
' Orchestrazione agenti (A8): Session Replay — registra gli eventi di una sessione agentica;
' REPLAYGET restituisce il "nastro" riproducibile. Runtime condiviso: ReplayRegistry / gmb_replay.c.
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.REPLAYSTART("sessione-demo")
  ok = AI.REPLAYRECORD("ask: spiega il DAG")
  ok = AI.REPLAYRECORD("tool: blackboard.write")
  ok = AI.REPLAYRECORD("answer: un DAG e' un grafo aciclico")
  ok = AI.REPLAYSTOP()
  PRINT "nastro: "; AI.REPLAYGET("sessione-demo")
  PRINT "replay: "; AI.REPLAYRUN("sessione-demo", "sessione-copia")
  PRINT "copia: "; AI.REPLAYGET("sessione-copia")
  PRINT "sessioni: "; AI.REPLAYLIST()
  ok = AI.REPLAYCLEAR()
END SUB
