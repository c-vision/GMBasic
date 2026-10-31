' POOL: riuso di oggetti (parita' Go sync.Pool: Get/Put).
IMPORT CONCURRENCY
IMPORT POOL

SUB Main()
  DIM p AS POOL
  DIM v AS STRING
  DIM w AS STRING
  POOL.PUT(p, "risorsa")
  v = POOL.GET(p)
  w = POOL.GET(p)
  IF v = "risorsa" AND w = "" THEN PRINT "POOL-OK" ELSE PRINT "FAIL:v="; v END IF
END SUB
