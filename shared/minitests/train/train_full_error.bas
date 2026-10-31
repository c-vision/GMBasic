' MiniTest: TRAIN FINETUNE {full:1} -> full-parameter (Adam su TUTTI i pesi, G6). Dispatch + errore catturabile.
' EXPECT: caught
IMPORT AI
SUB Main()
  MKDIR "bin"
  ON ERROR GOTO H
  TRAIN FINETUNE "bin/no_model", "{""full"":1,""dataset"":""bin/no.bin"",""ckpt"":""bin/k"",""steps"":2}"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
