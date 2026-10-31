' MiniTest: TRAIN RUNS — query del registro run (LLM.RUNSCOUNT()/RUNSJSON()).
' Processo fresco -> registro vuoto; verifica la parità 4 backend di conteggio e JSON.
' EXPECT: 0|delta|count0|denominate
IMPORT LLM
SUB Main()
  DIM c0 AS LONG = LLM.RUNSCOUNT()
  PRINT c0
  PRINT IIF(c0 >= 0, "delta", "no")
  DIM js AS STRING = LLM.RUNSJSON()
  PRINT IIF(INSTR(js, "count") > 0, "count0", "no")
  PRINT IIF(INSTR(js, "runs") > 0, "denominate", "no")
END SUB
