' AI.ORCHSAVE / AI.ORCHLOAD — persistenza su disco dello stato di orchestrazione (SQLite).
' Salva e riprende Artifact/Handoff/Metrics/Checkpoint/Replay in config/orchestration.db
' (o in un path esplicito). Runtime condiviso: OrchestrationPersistence (C#) / gmb_orchestration.c.
IMPORT AI

SUB Main
  DIM n AS DOUBLE
  n = AI.METRICRESET()
  n = AI.CHECKPOINTCLEAR()
  n = AI.CHECKPOINT("migrazione", "task=compiler;step=3")
  n = AI.HANDOFF("refactor", "coder")
  n = AI.METRICRECORD("coder", "tokens", 500)
  n = AI.ARTIFACTPUT("report", "/tmp/report.md")
  n = AI.ORCHSAVE("work/orchestration.db")
  PRINT "salvato (scope): "; n
  n = AI.CHECKPOINTCLEAR()
  n = AI.HANDOFFCLEAR()
  n = AI.METRICRESET()
  n = AI.ORCHLOAD("work/orchestration.db")
  PRINT "ripristinato (scope): "; n
  PRINT "checkpoint: "; AI.RESUME("migrazione")
  PRINT "token coder: "; AI.METRICGET("coder", "tokens")
END SUB
