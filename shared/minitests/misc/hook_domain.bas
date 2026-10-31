' MiniTest: orchestrazione agenti (A3) — lifecycle hooks (registro + fire/risoluzione).
' Deterministico: registra handler per eventi noti, li elenca, li risolve con HOOKFIRE e ne
' rimuove uno. Runtime condiviso Basic.Agents.HookRegistry (C#) / gmb_hook.c (C): stessa
' tassonomia e stesso formato JSON. L'invocazione automatica ai lifecycle point è il follow-on.
' EXPECT: reg= 1|events={"events":["before_task","after_task","before_agent","after_agent","before_tool","after_tool","before_model","after_model","on_error","on_retry","on_complete","on_verify","on_handoff"]}|list={"hooks":{"before_task":["Audit","OnBeforeTask"],"on_error":["OnError"]}}|fire={"event":"before_task","handlers":["Audit","OnBeforeTask"],"agents":{},"payload":"task1"}|unknown= 0|unreg= 1|list2={"hooks":{"before_task":["OnBeforeTask"],"on_error":["OnError"]}}
IMPORT AI

SUB Main
  DIM ok AS DOUBLE

  ok = AI.HOOKREGISTER("before_task", "OnBeforeTask")
  ok = AI.HOOKREGISTER("before_task", "Audit")
  ok = AI.HOOKREGISTER("on_error", "OnError")
  PRINT "reg="; ok

  PRINT "events="; AI.HOOKEVENTS()
  PRINT "list="; AI.HOOKLIST()
  PRINT "fire="; AI.HOOKFIRE("before_task", "task1")

  PRINT "unknown="; AI.HOOKREGISTER("bogus_event", "X")
  PRINT "unreg="; AI.HOOKUNREGISTER("before_task", "Audit")
  PRINT "list2="; AI.HOOKLIST()
END SUB
