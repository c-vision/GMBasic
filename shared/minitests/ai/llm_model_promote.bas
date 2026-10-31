' MiniTest: LLM MODEL PROMOTE — promuove il modello a residenza ACTIVE (tutti gli esperti
' residenti); su modello non caricato → errore CATTURABILE, su modello caricato → ok.
' Parità 4 backend.
' EXPECT: promote_before_err|load_ok|promote_after_ok|done
IMPORT LLM
SUB Main()
  ON ERROR GOTO E1
  LLM MODEL PROMOTE "m"
  PRINT "promote_before_ok"
  GOTO N1
E1:
  PRINT "promote_before_err"
N1:
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "load_ok"
  ON ERROR GOTO E2
  LLM MODEL PROMOTE "m"
  PRINT "promote_after_ok"
  GOTO N2
E2:
  PRINT "promote_after_err"
N2:
  LLM MODEL UNLOAD "m"
  PRINT "done"
END SUB
