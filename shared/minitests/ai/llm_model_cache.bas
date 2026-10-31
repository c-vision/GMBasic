' MiniTest: LLM MODEL CACHE — budget della expert cache (MB); su modello non caricato →
' errore CATTURABILE, su modello caricato → ok. Parità 4 backend.
' EXPECT: cache_before_err|load_ok|cache_after_ok|done
IMPORT LLM
SUB Main()
  ON ERROR GOTO E1
  LLM MODEL CACHE "m", 16
  PRINT "cache_before_ok"
  GOTO N1
E1:
  PRINT "cache_before_err"
N1:
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "load_ok"
  ON ERROR GOTO E2
  LLM MODEL CACHE "m", 16
  PRINT "cache_after_ok"
  GOTO N2
E2:
  PRINT "cache_after_err"
N2:
  LLM MODEL UNLOAD "m"
  PRINT "done"
END SUB
