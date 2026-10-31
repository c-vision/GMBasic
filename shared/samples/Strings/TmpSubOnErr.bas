DIM msg AS STRING
CALL TestaErrori(msg)
PRINT msg

SUB TestaErrori(msg AS STRING)
  ON ERROR GOTO OnErrHandler
  PRINT "prima"
  ERROR 1000, "x"
  PRINT "dopo errore"
  GOTO Fine
OnErrHandler:
  msg = "errore: " + STR$(ERR.Number)
  RESUME NEXT
Fine:
END SUB
