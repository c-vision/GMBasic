' MiniTest: LLM MODEL PREFETCH — prefetch speculativo degli esperti (topn); su modello non
' caricato → errore CATTURABILE, su modello caricato → ok. Parità 4 backend.
' EXPECT: prefetch_before_err|load_ok|prefetch_after_ok|done
IMPORT LLM
SUB Main()
  ON ERROR GOTO E1
  LLM MODEL PREFETCH "m", 4
  PRINT "prefetch_before_ok"
  GOTO N1
E1:
  PRINT "prefetch_before_err"
N1:
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "load_ok"
  ON ERROR GOTO E2
  LLM MODEL PREFETCH "m", 4
  PRINT "prefetch_after_ok"
  GOTO N2
E2:
  PRINT "prefetch_after_err"
N2:
  LLM MODEL UNLOAD "m"
  PRINT "done"
END SUB
