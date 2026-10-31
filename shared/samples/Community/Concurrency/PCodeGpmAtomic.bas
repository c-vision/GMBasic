' PCodeVM: ATOMIC (parità Go sync/atomic) — 4 goroutine incrementano un contatore ATOMIC
' con ATOMIC.INCREMENT (AddInt64); anche senza mutex il totale deve essere deterministico
' (4 goroutine x 100 incrementi = 400). Verifica anche FETCHVALUE, STORE e COMPARESWAP.
IMPORT CONCURRENCY
IMPORT ATOMIC

SUB Worker(c AS ATOMIC, wg AS WAITGROUP, n AS LONG)
  DIM i AS LONG
  FOR i = 1 TO n
    ATOMIC.INCREMENT(c, 1)
  NEXT i
END SUB

SUB Main()
  DIM c AS ATOMIC
  DIM wg AS WAITGROUP
  DIM v AS LONG
  DIM sw AS BIT
  GO SUB Worker(c, wg, 100) IN wg
  GO SUB Worker(c, wg, 100) IN wg
  GO SUB Worker(c, wg, 100) IN wg
  GO SUB Worker(c, wg, 100) IN wg
  WAIT wg
  v = ATOMIC.FETCHVALUE(c)
  ATOMIC.STORE(c, 42)
  sw = ATOMIC.COMPARESWAP(c, 42, 7)
  IF v = 400 AND sw = -1 AND ATOMIC.FETCHVALUE(c) = 7 THEN
    PRINT "PCODE-GPM-ATOMIC-OK"
  ELSE
    PRINT "FAIL:v="; v; " sw="; sw
  END IF
END SUB
