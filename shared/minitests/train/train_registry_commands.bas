' MiniTest: comandi registro/config TRAIN — RUNS LIST/SHOW/PRUNE, CONFIG SHOW/SET, STATUS, LORA INFO.
' EXPECT: {"rank":4,"layer":15,"steps":20,"batch":4,"seed":7,"lr":0.01000}|{"rank":4,"layer":15,"steps":20,"batch":4,"seed":7,"lr":0.01000}|steps20|{"count":0,"runs":[]}|{"count":0,"runs":[]}|{"count":0,"adapters":[]}|0|0
IMPORT AI
IMPORT LLM
SUB Main()
  TRAIN CONFIG SET "steps", "20"
  TRAIN CONFIG SHOW
  PRINT IIF(INSTR(LLM.TRAINCONFIG(), "20") > 0, "steps20", "no")
  TRAIN RUNS LIST
  TRAIN STATUS
  TRAIN STATUS "1"
  TRAIN RUNS SHOW "1"
  TRAIN LORA INFO
  TRAIN RUNS PRUNE
  PRINT LLM.RUNSCOUNT()
END SUB
