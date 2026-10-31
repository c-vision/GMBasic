' MiniTest: TRAIN DISTILL TRAIN con {teacher:...} -> distill teacher-student KL (dispatch).
' Dataset/modello inesistenti -> errore CATTURABILE su tutti i backend (dispatch ok).
' EXPECT: caught
IMPORT AI
SUB Main()
  MKDIR "bin"
  ON ERROR GOTO H
  TRAIN DISTILL TRAIN "bin/no.bin", "{""model"":""bin/no_model"",""teacher"":""bin/no_teacher"",""ckpt"":""bin/kl""}"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
