' MiniTest: TRAIN CONFIG SET — LLM.TRAINCONFIGSET aggiorna un default; SHOW lo riflette.
' EXPECT: set|persist
IMPORT LLM
SUB Main()
  DIM js AS STRING = LLM.TRAINCONFIGSET("steps", "1337")
  PRINT IIF(INSTR(js, "1337") > 0, "set", "no")
  PRINT IIF(INSTR(LLM.TRAINCONFIG(), "1337") > 0, "persist", "no")
END SUB
