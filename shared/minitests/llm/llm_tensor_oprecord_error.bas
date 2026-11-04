' MiniTest: LLM TENSOR OPRECORD su file inesistente -> errore CATTURABILE su 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR OPRECORD "work/no.gguf", "work/op.json"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
