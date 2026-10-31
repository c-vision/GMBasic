' MiniTest: TRAIN LORA LIST — LLM.LORALIST() (adapter dal registro, vuoto a inizio processo).
' EXPECT: count|adapters
IMPORT LLM
SUB Main()
  DIM js AS STRING = LLM.LORALIST()
  PRINT IIF(INSTR(js, "count") > 0, "count", "no")
  PRINT IIF(INSTR(js, "adapters") > 0, "adapters", "no")
END SUB
