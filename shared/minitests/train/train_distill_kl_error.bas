' MiniTest: TRAIN DISTILL TRAIN con {teacher:...} -> distill teacher-student KL (dispatch).
' Dataset/modello inesistenti -> errore CATTURABILE su tutti i backend (dispatch ok).
' EXPECT: caught
IMPORT AI
SUB Main()
  CHDIR PROJECTFOLDER()
  MKDIR "work"
  ON ERROR GOTO H
  TRAIN DISTILL TRAIN "work/no.bin", "{""model"":""work/no_model"",""teacher"":""work/no_teacher"",""ckpt"":""work/kl""}"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
