' AI.RESOURCESET / RESOURCEGET / RESOURCELIST / RESOURCEAVAILABLE / RESOURCESWARMSIZE
' Orchestrazione agenti (A7): Resource Manager — limiti (max_agents, model_slots, ...) e
' dynamic swarm sizing: il numero effettivo di agenti è min(richiesti, max_agents).
' Runtime condiviso: Basic.Agents.ResourceRegistry (C#) / gmb_resource.c (C).
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.RESOURCESET("max_agents", "4")
  ok = AI.RESOURCESET("model_slots", "2")
  ok = AI.RESOURCESET("context_budget", "32768")

  PRINT "max_agents="; AI.RESOURCEGET("max_agents")
  PRINT "swarm(10)="; AI.RESOURCESWARMSIZE(10)
  PRINT "model_slots disponibili: "; AI.RESOURCEAVAILABLE("model_slots")
  PRINT AI.RESOURCELIST()
END SUB
