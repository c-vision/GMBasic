' MiniTest: orchestrazione (A7) — Resource Manager (registro limiti + dynamic swarm sizing).
' Deterministico. Runtime condiviso Basic.Agents.ResourceRegistry (C#) / gmb_resource.c (C).
' EXPECT: set= 1|get=2|avail= 1  0|swarm= 2  1|list={"resources":{"max_agents":"2","model_slots":"1"}}
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.RESOURCESET("max_agents", "2")
  ok = AI.RESOURCESET("model_slots", "1")
  PRINT "set="; ok
  PRINT "get="; AI.RESOURCEGET("max_agents")
  PRINT "avail="; AI.RESOURCEAVAILABLE("model_slots"); AI.RESOURCEAVAILABLE("gpu")
  PRINT "swarm="; AI.RESOURCESWARMSIZE(5); AI.RESOURCESWARMSIZE(1)
  PRINT "list="; AI.RESOURCELIST()
END SUB
