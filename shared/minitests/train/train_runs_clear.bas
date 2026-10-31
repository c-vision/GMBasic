' MiniTest: TRAIN RUNS PRUNE — LLM.RUNSCLEAR() svuota il registro e ritorna le run rimosse.
' EXPECT: 0|zero
IMPORT LLM
SUB Main()
  DIM n AS LONG = LLM.RUNSCLEAR()
  PRINT n
  PRINT IIF(LLM.RUNSCOUNT() = 0, "zero", "no")
END SUB
