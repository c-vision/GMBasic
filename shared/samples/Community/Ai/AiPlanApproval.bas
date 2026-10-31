' AI.PLANDEFINE / PLANADD / PLANAPPROVE / PLANSTATUS / PLANLIST / PLANRUN
' Orchestrazione agenti (A2): Planner — un piano associa task a un obiettivo e richiede
' l'approvazione prima dell'esecuzione (separa pianificazione da esecuzione).
' Runtime condiviso: Basic.Agents.PlanRegistry (C#) / gmb_plan.c (C).
IMPORT AI

SUB Main
  DIM ok AS DOUBLE

  ok = AI.TASKDEFINE("analisi", "analizza i requisiti")
  ok = AI.TASKDEFINE("sintesi", "sintetizza la soluzione")
  ok = AI.TASKDEPENDS("sintesi", "analisi")

  ok = AI.PLANDEFINE("piano1", "progettare una feature")
  ok = AI.PLANADD("piano1", "analisi")
  ok = AI.PLANADD("piano1", "sintesi")

  PRINT "piani: "; AI.PLANLIST()
  PRINT "piano (draft): "; AI.PLANSTATUS("piano1")
  PRINT "run senza approvazione: "; AI.PLANRUN("piano1")

  ok = AI.PLANAPPROVE("piano1")
  PRINT "piano (approvato): "; AI.PLANSTATUS("piano1")
  PRINT "esecuzione:"
  PRINT AI.PLANRUN("piano1")
END SUB
