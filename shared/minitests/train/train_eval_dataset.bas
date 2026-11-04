' MiniTest: TRAIN EVAL <model>, <dataset> (§4: NLL media su un dataset multi-prompt) su modello
' inesistente -> errore catturabile, parità 4 backend.
' TAG: heavy
' EXPECT: caught
IMPORT AI
IMPORT LLM
IMPORT IO
SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  DIM d AS STRING = "/tmp/gmb_eval_ds.jsonl"
  DIM h AS LONG = IO.FCREATE(d)
  IO.FWRITE(h, "the cat sat" + CHR(10) + "two plus two" + CHR(10))
  IO.FCLOSE(h)
  ON ERROR GOTO H
  TRAIN EVAL GMBWORK$ + "/no_model", "/tmp/gmb_eval_ds.jsonl"
  PRINT "no_err"
  GOTO D
H:
  PRINT "caught"
D:
  ON ERROR RESUME NEXT
  KILL d
  ON ERROR GOTO 0
END SUB
