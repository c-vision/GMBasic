' MiniTest: TRAIN DATASET STATS — LLM.DATASETSTATS(path) riporta i byte del file (parità 4 backend).
' EXPECT: missing|exists
IMPORT LLM
IMPORT IO
SUB Main()
  DIM p AS STRING = "/tmp/gmb_train_stats.bin"
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0
  DIM js1 AS STRING = LLM.DATASETSTATS(p)
  PRINT IIF(INSTR(js1, "-1") > 0, "missing", "no")

  DIM h AS LONG = IO.FCREATE(p)
  IO.FWRITE(h, "abcdefghij")
  IO.FCLOSE(h)
  DIM js2 AS STRING = LLM.DATASETSTATS(p)
  PRINT IIF(INSTR(js2, "10") > 0, "exists", "no")

  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0
END SUB
