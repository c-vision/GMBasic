' MiniTest: TRAIN EVAL GATE (gate ref/cand dedicato) + TRAIN EXPORT/STOP (§4).
' EXPECT: 0|0|0|0|{"verdict":"ERROR","reason":"ref model not loadable"}|gate
IMPORT AI
IMPORT LLM
SUB Main()
  CHDIR PROJECTFOLDER()
  PRINT LLM.TRAINEXPORT("work/exp")
  PRINT LLM.RUNSTOP("99")
  TRAIN EXPORT "work/exp"
  TRAIN STOP "1"
  TRAIN EVAL GATE "work/no_model", "work/no_cand"
  PRINT IIF(INSTR(LLM.EVALGATE("work/a", "work/b"), "ERROR") > 0, "gate", "no")
END SUB
