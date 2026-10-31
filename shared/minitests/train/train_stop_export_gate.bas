' MiniTest: TRAIN EVAL GATE (gate ref/cand dedicato) + TRAIN EXPORT/STOP (§4).
' EXPECT: 0|0|0|0|{"verdict":"ERROR","reason":"ref model not loadable"}|gate
IMPORT AI
IMPORT LLM
SUB Main()
  PRINT LLM.TRAINEXPORT("bin/exp")
  PRINT LLM.RUNSTOP("99")
  TRAIN EXPORT "bin/exp"
  TRAIN STOP "1"
  TRAIN EVAL GATE "bin/no_model", "bin/no_cand"
  PRINT IIF(INSTR(LLM.EVALGATE("bin/a", "bin/b"), "ERROR") > 0, "gate", "no")
END SUB
