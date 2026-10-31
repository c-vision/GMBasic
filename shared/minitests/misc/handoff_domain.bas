' MiniTest: orchestrazione (A9) — Handoff (task → agente destinatario, stato + risultato).
' Deterministico. Runtime condiviso Basic.Agents.HandoffRegistry (C#) / gmb_handoff.c (C).
' EXPECT: set= 1|status={"task":"t1","to":"agentA","status":"pending","result":""}|complete= 1|list={"handoffs":[{"task":"t1","to":"agentA","status":"done","result":"fatto"},{"task":"t2","to":"agentB","status":"pending","result":""}]}|clear= 2|list2={"handoffs":[]}
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.HANDOFF("t1", "agentA")
  ok = AI.HANDOFF("t2", "agentB")
  PRINT "set="; ok
  PRINT "status="; AI.HANDOFFSTATUS("t1")
  ok = AI.HANDOFFCOMPLETE("t1", "fatto")
  PRINT "complete="; ok
  PRINT "list="; AI.HANDOFFLIST()
  PRINT "clear="; AI.HANDOFFCLEAR()
  PRINT "list2="; AI.HANDOFFLIST()
END SUB
