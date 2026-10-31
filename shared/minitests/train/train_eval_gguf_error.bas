' MiniTest: TRAIN EVAL <model.gguf>, <dataset> su un .gguf inesistente -> errore CATTURABILE.
' Verifica il ramo GGUF (tokenizer ricostruito dal GGUF): path assente -> errore, non hang.
' EXPECT: caught
IMPORT LLM
SUB Main()
  ON ERROR GOTO H
  TRAIN EVAL "bin/no_model.gguf", "bin/no_ds.txt"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
