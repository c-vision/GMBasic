' MiniTest NAMESPACE POOL — entrambe le funzioni della superficie (PUT, GET).
' Semantica sync.Pool: PUT deposita, il primo GET ritira l'oggetto, il secondo
' (pool vuoto) ritorna il valore zero del tipo.
' EXPECT: put=get=ok|empty=ok|num= 42
IMPORT CONCURRENCY
IMPORT POOL

SUB Main()
  DIM p AS POOL
  DIM got AS STRING
  DIM none AS STRING
  POOL.PUT(p, "risorsa")
  got = POOL.GET(p)
  none = POOL.GET(p)
  IF got = "risorsa" AND none = "" THEN PRINT "put=get=ok" ELSE PRINT "put=get=ko"

  DIM empty AS STRING
  DIM p2 AS POOL
  empty = POOL.GET(p2)
  IF empty = "" THEN PRINT "empty=ok" ELSE PRINT "empty=ko"

  ' Valore NUMERICO: GET restituisce il valore tipizzato (non il wrapper POOL).
  DIM q AS POOL
  DIM n AS LONG
  POOL.PUT(q, 42)
  n = POOL.GET(q)
  PRINT "num="; n
END SUB
