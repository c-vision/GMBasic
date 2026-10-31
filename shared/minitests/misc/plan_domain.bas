' MiniTest: orchestrazione agenti (A2) — Planner (piani con approvazione).
' Deterministico: i task senza agente sono passthrough; PLANRUN rifiuta un piano non approvato
' e, dopo PLANAPPROVE, esegue il Task Graph associato. Runtime condiviso
' Basic.Agents.PlanRegistry (C#) / gmb_plan.c (C): stessa semantica.
' EXPECT: defined= 1|draft={"name":"p1","goal":"obiettivo demo","approved":false,"status":"draft","result":"","tasks":["t1","t2"]}|not-approved={"error":"piano non approvato (usare PLANAPPROVE)"}|approved={"tasks":[{"id":"t1","desc":"analisi","agent":"","status":"done","result":"analisi","deps":[]},{"id":"t2","desc":"sintesi","agent":"","status":"done","result":"sintesi","deps":["t1"]}]}
IMPORT AI

SUB Main
  AI.TASKCLEAR()
  AI.PLANCLEAR()
  DIM ok AS DOUBLE
  DIM s AS STRING

  ok = AI.TASKDEFINE("t1", "analisi")
  ok = AI.TASKDEFINE("t2", "sintesi")
  ok = AI.TASKDEPENDS("t2", "t1")

  ok = AI.PLANDEFINE("p1", "obiettivo demo")
  ok = AI.PLANADD("p1", "t1")
  ok = AI.PLANADD("p1", "t2")
  PRINT "defined="; ok

  s = AI.PLANSTATUS("p1")
  PRINT "draft="; s

  s = AI.PLANRUN("p1")
  PRINT "not-approved="; s

  ok = AI.PLANAPPROVE("p1")
  s = AI.PLANRUN("p1")
  PRINT "approved="; s
END SUB
