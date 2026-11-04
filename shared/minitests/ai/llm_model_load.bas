' MiniTest: LLM MODEL LOAD — carica un modello MoE GGUF (fixture), errore CATTURABILE su path
' inesistente, variante RESIDENT (tutto in RAM) vs default SWAP (streaming esperti da disco).
' Parità 4 backend.
' EXPECT: load_ok|bad_err|resident_ok|done
IMPORT LLM
SUB Main()
  ON ERROR GOTO E1
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf"
  PRINT "load_ok"
  LLM MODEL UNLOAD "m"
  GOTO N1
E1:
  PRINT "load_err"
N1:
  ON ERROR GOTO E2
  LLM MODEL LOAD "m", "work/does-not-exist.gguf"
  PRINT "bad_ok"
  GOTO N2
E2:
  PRINT "bad_err"
N2:
  ON ERROR GOTO E3
  LLM MODEL LOAD "m", "../../samples/Fixtures/models/tiny_moe.gguf", RESIDENT
  PRINT "resident_ok"
  LLM MODEL UNLOAD "m"
  GOTO N3
E3:
  PRINT "resident_err"
N3:
  PRINT "done"
END SUB
