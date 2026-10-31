' MiniTest: LLM MODEL DEMOTE — riporta il modello a residenza WARM (esperti on-demand da disco);
' su modello non caricato → errore CATTURABILE, su modello caricato → ok. Parità 4 backend.
' EXPECT: demote_before_err|load_ok|demote_after_ok|done
IMPORT LLM
SUB Main()
  ON ERROR GOTO E1
  LLM MODEL DEMOTE "m"
  PRINT "demote_before_ok"
  GOTO N1
E1:
  PRINT "demote_before_err"
N1:
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "load_ok"
  ON ERROR GOTO E2
  LLM MODEL DEMOTE "m"
  PRINT "demote_after_ok"
  GOTO N2
E2:
  PRINT "demote_after_err"
N2:
  LLM MODEL UNLOAD "m"
  PRINT "done"
END SUB
