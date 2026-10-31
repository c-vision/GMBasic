' MiniTest: LLM TENSOR SENSITIVITY — sensibilità empirica ΔPPL per ruolo (6.1/S2). Dispatch + errore catturabile.
' EXPECT: caught
IMPORT LLM
SUB Main()
  MKDIR "bin"
  ON ERROR GOTO H
  LLM TENSOR SENSITIVITY "bin/no_model.gguf", "bin/no.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
