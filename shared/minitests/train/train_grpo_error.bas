' MiniTest: TRAIN GRPO (RLHF senza critic / RLVR — G-opt2). Dispatch 4 backend + errore catturabile.
' EXPECT: caught
IMPORT AI
SUB Main()
  MKDIR "work"
  ON ERROR GOTO H
  TRAIN GRPO "work/no_model", "{""dataset"":""work/no.bin"",""ckpt"":""work/g"",""steps"":2,""group"":2}"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
