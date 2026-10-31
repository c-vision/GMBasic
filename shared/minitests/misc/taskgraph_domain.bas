' MiniTest: orchestrazione agenti (B1) — Task Graph con dipendenze.
' Solo operazioni DETERMINISTICHE (DEFINE/DEPENDS/ASSIGN/STATUS/LIST/RUN): i nodi senza agente
' usano la descrizione come risultato (passthrough), quindi RUN non richiede LLM. Runtime
' condiviso Basic.Agents.TaskGraphRegistry (C#) / gmb_taskgraph.c (C): stessa semantica.
' EXPECT: defined= 1|status-a={"id":"a","desc":"step-a","agent":"","status":"pending","result":"","deps":[]}|list={"tasks":[{"id":"a","desc":"step-a","agent":"","status":"pending","result":"","deps":[]},{"id":"b","desc":"step-b","agent":"","status":"pending","result":"","deps":["a"]},{"id":"c","desc":"step-c","agent":"","status":"pending","result":"","deps":["b"]}]}|run={"tasks":[{"id":"a","desc":"step-a","agent":"","status":"done","result":"step-a","deps":[]},{"id":"b","desc":"step-b","agent":"","status":"done","result":"step-b","deps":["a"]},{"id":"c","desc":"step-c","agent":"","status":"done","result":"step-c","deps":["b"]}]}
IMPORT AI

SUB Main
  AI.TASKCLEAR()
  DIM ok AS DOUBLE
  DIM s AS STRING

  ok = AI.TASKDEFINE("a", "step-a")
  ok = AI.TASKDEFINE("b", "step-b")
  ok = AI.TASKDEFINE("c", "step-c")
  ok = AI.TASKDEPENDS("b", "a")
  ok = AI.TASKDEPENDS("c", "b")
  PRINT "defined="; ok

  s = AI.TASKSTATUS("a")
  PRINT "status-a="; s

  s = AI.TASKLIST()
  PRINT "list="; s

  s = AI.TASKRUN()
  PRINT "run="; s
END SUB
