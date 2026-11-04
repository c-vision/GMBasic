' MiniTest: LLM MODEL (MoE out-of-core, GGUF) — superficie residenza/streaming.
' LOAD con percorso inesistente → errore CATTURABILE (mai stub silenzioso); STREAMSTATS su
' modello non caricato → errore catturabile. Parità 4 backend.
' EXPECT: load_err|ss_err|done
IMPORT LLM
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR GOTO OnErr
  LLM MODEL LOAD "m", GMBWORK$ + "/does-not-exist.gguf"
  PRINT "loaded"
  GOTO Fine
OnErr:
  PRINT "load_err"
Fine:
  ON ERROR GOTO OnErr2
  PRINT LLM.STREAMSTATS("m")
  PRINT "ss_ok"
  GOTO Fine2
OnErr2:
  PRINT "ss_err"
Fine2:
  PRINT "done"
END SUB
