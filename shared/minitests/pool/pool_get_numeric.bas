' MiniTest POOL.GET con valore numerico: restituisce il valore tipizzato, non il wrapper POOL.
' EXPECT: n= 42|s=ciao
IMPORT CONCURRENCY
IMPORT POOL

SUB Main()
  DIM p AS POOL
  DIM n AS LONG
  POOL.PUT(p, 42)
  n = POOL.GET(p)
  PRINT "n="; n
  DIM q AS POOL
  DIM s AS STRING
  POOL.PUT(q, "ciao")
  s = POOL.GET(q)
  PRINT "s="; s
END SUB
