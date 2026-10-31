' MiniTest: LLM MODEL UNLOAD — scarica un modello caricato; un secondo UNLOAD è un errore
' CATTURABILE (mai stub silenzioso). Parità 4 backend.
' EXPECT: load_ok|unload_ok|unload2_err|done
IMPORT LLM
SUB Main()
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "load_ok"
  ON ERROR GOTO E1
  LLM MODEL UNLOAD "m"
  PRINT "unload_ok"
  GOTO N1
E1:
  PRINT "unload_err"
N1:
  ON ERROR GOTO E2
  LLM MODEL UNLOAD "m"
  PRINT "unload2_ok"
  GOTO N2
E2:
  PRINT "unload2_err"
N2:
  PRINT "done"
END SUB
