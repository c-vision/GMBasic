' AI.CHECKPOINT / RESUME / CHECKPOINTLIST / CHECKPOINTCLEAR — checkpoint/recovery (B7).
' Salva lo stato di una sessione agentica e lo riprende dopo un'interruzione.
' Runtime condiviso: Basic.Agents.CheckpointRegistry / gmb_checkpoint.c.
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.CHECKPOINT("migrazione", "task=compiler;step=3")
  PRINT "riprende: "; AI.RESUME("migrazione")
  PRINT "checkpoint: "; AI.CHECKPOINTLIST()
  ok = AI.CHECKPOINTCLEAR()
END SUB
