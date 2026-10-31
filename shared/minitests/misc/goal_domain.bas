' MiniTest: orchestrazione (A4) — Goal persistente + idle continuation.
' Deterministico: GOALSET/GET/TICK/STATUS/CLEAR. TICK = impulso di continuazione idle.
' Runtime condiviso Basic.Agents.GoalRegistry (C#) / gmb_goal.c (C).
' EXPECT: set= 1|get=completa il refactor|tick1= 1|tick2= 2|status={"objective":"completa il refactor","active":true,"steps":2,"agent":"","last":""}|clear= 1|tick-after= 0
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.GOALSET("completa il refactor")
  PRINT "set="; ok
  PRINT "get="; AI.GOALGET()
  PRINT "tick1="; AI.GOALTICK()
  PRINT "tick2="; AI.GOALTICK()
  PRINT "status="; AI.GOALSTATUS()
  PRINT "clear="; AI.GOALCLEAR()
  PRINT "tick-after="; AI.GOALTICK()
END SUB
