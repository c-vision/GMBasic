' AI.GOALSET / GOALAGENT / GOALGET / GOALSTATUS / GOALTICK / GOALCLEAR
' Orchestrazione agenti (A4): Goal persistente + idle continuation — un obiettivo di thread che
' il runtime può riprendere quando l'agente va idle (GOALTICK = impulso di continuazione).
' GOALAGENT associa un agente: TICK lo interroga con l'obiettivo (aggancio GPM).
' Runtime condiviso: Basic.Agents.GoalRegistry (C#) / gmb_goal.c (C).
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  AI AGENT CREATE "goal-agent"
  ok = AI.GOALSET("portare a termine la migrazione del compilatore")
  PRINT "agente: "; AI.GOALAGENT("goal-agent")
  PRINT "obiettivo: "; AI.GOALGET()
  PRINT "continuazione 1: "; AI.GOALTICK()
  PRINT "continuazione 2: "; AI.GOALTICK()
  PRINT "stato: "; AI.GOALSTATUS()
  ok = AI.GOALCLEAR()
  PRINT "goal chiuso: "; ok
END SUB
