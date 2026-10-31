' AI.HOOKREGISTER / HOOKUNREGISTER / HOOKLIST / HOOKEVENTS / HOOKFIRE
' Orchestrazione agenti (A3): lifecycle hooks — registro evento → handler (nomi di SUB).
' HOOKFIRE risolve gli handler registrati per l'evento; quelli che sono AGENTI vengono
' dispacciati (aggancio GPM). Runtime condiviso: Basic.Agents.HookRegistry / gmb_hook.c.
IMPORT AI

' Handler-SUB di un hook: un hook è un EVENTO con tassonomia fissa; i SUB si registrano con
' ADD HANDLER e si scatenano con RAISE EVENT (base eventi esistente, dispatch per nome).
SUB OnBeforeTask(payload AS STRING)
  PRINT "SUB before_task: "; payload
END SUB

SUB Main
  DIM ok AS DOUBLE

  PRINT "eventi noti: "; AI.HOOKEVENTS()

  ADD HANDLER before_task, OnBeforeTask

  AI AGENT CREATE "Audit"
  ok = AI.HOOKREGISTER("before_task", "Audit")
  ok = AI.HOOKREGISTER("before_task", "ValidaTask")
  ok = AI.HOOKREGISTER("after_task", "PubblicaRisultato")
  ok = AI.HOOKREGISTER("on_error", "LogErrore")
  PRINT "registrati: "; AI.HOOKLIST()

  PRINT "fire before_task: "; AI.HOOKFIRE("before_task", "refactor-compiler")
  RAISE EVENT before_task("refactor-compiler")
  PRINT "fire on_verify: "; AI.HOOKFIRE("on_verify", "suite=42")

  ok = AI.HOOKUNREGISTER("on_error", "LogErrore")
  PRINT "dopo unregister: "; AI.HOOKLIST()
END SUB
