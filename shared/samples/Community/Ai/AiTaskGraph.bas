' AI.TASKDEFINE / TASKDEPENDS / TASKASSIGN / TASKSTATUS / TASKLIST / TASKRUN
' Orchestrazione agenti (B1): Task Graph con dipendenze eseguito in ordine topologico.
' Nodi senza agente = passthrough (risultato = descrizione); con TASKASSIGN l'agente viene
' interrogato col prompt composto da descrizione + risultati delle dipendenze.
' Runtime condiviso: Basic.Agents.TaskGraphRegistry (C#) / gmb_taskgraph.c (C).
IMPORT AI

SUB Main
  DIM ok AS DOUBLE

  ok = AI.TASKDEFINE("research", "raccogli i requisiti")
  ok = AI.TASKDEFINE("design", "progetta la soluzione")
  ok = AI.TASKDEFINE("impl", "implementa la soluzione")
  ok = AI.TASKDEPENDS("design", "research")
  ok = AI.TASKDEPENDS("impl", "design")
  ok = AI.TASKASSIGN("research", "researcher")
  ok = AI.TASKASSIGN("impl", "coder")

  PRINT "stato research: "; AI.TASKSTATUS("research")
  PRINT AI.TASKLIST()
  PRINT "esecuzione:"
  PRINT AI.TASKRUN()
END SUB
