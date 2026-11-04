' MiniTest: LLM TENSOR COMPILE su file inesistente -> errore CATTURABILE su tutti i 4 backend.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  LLM TENSOR COMPILE "work/no.gguf", "work/out.bin"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
