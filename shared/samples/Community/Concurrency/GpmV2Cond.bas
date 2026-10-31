' COND: variabile di condizione (parita' Go sync.Cond: Wait/Signal).
IMPORT CONCURRENCY

DIM Ready AS BIT = 0

SUB Consumatore(c AS COND, mu AS MUTEX, W AS WAITGROUP)
  MUTEX LOCK mu
  WHILE Ready = 0
    COND WAIT c, mu
  WEND
  PRINT "CONSUMATORE-SVEGLIATO"
  MUTEX UNLOCK mu
END SUB

SUB Main()
  DIM c AS COND
  DIM mu AS MUTEX
  DIM W AS WAITGROUP
  GO SUB Consumatore(c, mu, W) IN W
  SLEEP 50
  MUTEX LOCK mu
  Ready = 1
  COND SIGNAL c
  MUTEX UNLOCK mu
  WAIT W
  PRINT "COND-DONE"
END SUB
