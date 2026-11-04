' MiniTest: LLM QUANT <in>, <out>, <bpw> su file inesistente -> errore CATTURABILE (4 backend).
' Il ramo a 3 argomenti (auto) deve fallire con errore catturabile, come il ramo a 2 argomenti.
' EXPECT: caught
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO H
  LLM QUANT GMBWORK$ + "/no.gguf", GMBWORK$ + "/out.gguf", 4.5
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
END SUB
