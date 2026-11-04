' MiniTest: TRAIN FINETUNE {full:1} -> full-parameter (Adam su TUTTI i pesi, G6). Dispatch + errore catturabile.
' EXPECT: caught
IMPORT AI
SUB Main()
  CHDIR PROJECTFOLDER()
  MKDIR "work"
  ON ERROR GOTO H
  TRAIN FINETUNE "work/no_model", "{""full"":1,""dataset"":""work/no.bin"",""ckpt"":""work/k"",""steps"":2}"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
