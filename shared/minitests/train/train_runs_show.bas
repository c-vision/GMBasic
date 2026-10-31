' MiniTest: TRAIN RUNS SHOW — LLM.RUNSITEM(id) su registro vuoto ritorna stringa vuota.
' EXPECT: empty|zero
IMPORT LLM
SUB Main()
  DIM js AS STRING = LLM.RUNSITEM("1")
  PRINT IIF(LEN(js) = 0, "empty", "no")
  PRINT IIF(LLM.RUNSCOUNT() = 0, "zero", "no")
END SUB
