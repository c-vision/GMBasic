' MiniTest: TRAIN CONFIG SHOW — LLM.TRAINCONFIG() espone i default di sessione (JSON).
' EXPECT: rank|steps
IMPORT LLM
SUB Main()
  DIM js AS STRING = LLM.TRAINCONFIG()
  PRINT IIF(INSTR(js, "rank") > 0, "rank", "no")
  PRINT IIF(INSTR(js, "steps") > 0, "steps", "no")
END SUB
