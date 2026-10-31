' MiniTest: orchestrazione (B7) — checkpoint/recovery (salva e riprendi stato sessione).
' Deterministico. Runtime condiviso Basic.Agents.CheckpointRegistry (C#) / gmb_checkpoint.c (C).
' EXPECT: save= 1|resume=step=3;agent=coder|list={"checkpoints":[{"session":"s1","state":"step=3;agent=coder"}]}|clear= 1
IMPORT AI

SUB Main
  DIM ok AS DOUBLE
  ok = AI.CHECKPOINT("s1", "step=3;agent=coder")
  PRINT "save="; ok
  PRINT "resume="; AI.RESUME("s1")
  PRINT "list="; AI.CHECKPOINTLIST()
  PRINT "clear="; AI.CHECKPOINTCLEAR()
END SUB
