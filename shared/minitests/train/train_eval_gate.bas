' MiniTest: TRAIN EVAL GATE — comando gate ref/cand DEDICATO (LLM.EVALGATE).
' Confronta la perplexity di cand vs ref; modelli assenti → verdetto ERROR (deterministico).
' EXPECT: {"verdict":"ERROR","reason":"ref model not loadable"}|ok
IMPORT AI
IMPORT LLM
SUB Main()
  TRAIN EVAL GATE "work/no_ref", "work/no_cand"
  PRINT IIF(INSTR(LLM.EVALGATE("work/a", "work/b"), "ERROR") > 0, "ok", "no")
END SUB
