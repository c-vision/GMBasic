' MiniTest: persistenza orchestrazione su disco (SQLite) — save/load di tutti i registri.
' Deterministico (l'harness azzera i registri tra i backend). Runtime condiviso:
' Basic.Agents.OrchestrationPersistence (C#) / gmb_orchestration.c (C).
' EXPECT: save= 11|load= 11|checkpoint=step=3|metric= 100|artifact=/tmp/r.md|category=provX|resource=4|rules=regola|task={"id":"tk1","desc":"step","agent":"","status":"pending","result":"","deps":[]}|plan={"name":"pl1","goal":"goal","approved":false,"status":"draft","result":"","tasks":[]}|hook={"event":"before_task","handlers":["Handler"],"agents":{},"payload":"x"}|err= 0
IMPORT AI

SUB Main
  DIM n AS DOUBLE
  n = AI.CHECKPOINT("s1", "step=3")
  n = AI.HANDOFF("t1", "agentA")
  n = AI.METRICRECORD("a1", "tokens", 100)
  n = AI.ARTIFACTPUT("rep", "/tmp/r.md")
  n = AI.CATEGORYSET("ultrabrain", "provX")
  n = AI.CATEGORYAUTO(1)
  n = AI.RESOURCESET("max_agents", "4")
  n = AI.RULESADD("/proj", "regola")
  n = AI.TASKDEFINE("tk1", "step")
  n = AI.PLANDEFINE("pl1", "goal")
  n = AI.HOOKREGISTER("before_task", "Handler")
  n = AI.ORCHSAVE("/tmp/gmb_orch_test.db")
  PRINT "save="; n
  n = AI.CHECKPOINTCLEAR()
  n = AI.HANDOFFCLEAR()
  n = AI.METRICRESET()
  n = AI.ARTIFACTREMOVE("rep")
  n = AI.ORCHLOAD("/tmp/gmb_orch_test.db")
  PRINT "load="; n
  PRINT "checkpoint="; AI.RESUME("s1")
  PRINT "metric="; AI.METRICGET("a1", "tokens")
  PRINT "artifact="; AI.ARTIFACTGET("rep")
  PRINT "category="; AI.CATEGORYPICK("ultrabrain")
  PRINT "resource="; AI.RESOURCEGET("max_agents")
  PRINT "rules="; AI.RULESFOR("/proj/src/f.bas")
  PRINT "task="; AI.TASKSTATUS("tk1")
  PRINT "plan="; AI.PLANSTATUS("pl1")
  PRINT "hook="; AI.HOOKFIRE("before_task", "x")
  PRINT "err="; AI.ORCHSAVE("/nonexistent_root_dir_xyz/foo.db")
END SUB
