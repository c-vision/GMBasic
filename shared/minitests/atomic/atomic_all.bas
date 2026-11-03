' MiniTest NAMESPACE ATOMIC — tutte le funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: inc-ok|fetch-ok|cmp-ok|swap-ok
IMPORT CONCURRENCY
IMPORT ATOMIC

SUB T_atomic()
  DIM c AS ATOMIC
  DIM v AS LONG
  DIM sw AS BIT
  ATOMIC.INCREMENT(c, 5)
  ATOMIC.INCREMENT(c, 3)
  v = ATOMIC.FETCHVALUE(c)
  IF v = 8 THEN PRINT "inc-ok"
  ATOMIC.STORE(c, 42)
  IF ATOMIC.FETCHVALUE(c) = 42 THEN PRINT "fetch-ok"
  sw = ATOMIC.COMPARESWAP(c, 42, 7)
  IF sw <> 0 AND ATOMIC.FETCHVALUE(c) = 7 THEN PRINT "cmp-ok"
  v = ATOMIC.SWAP(c, 99)
  IF v = 7 AND ATOMIC.FETCHVALUE(c) = 99 THEN PRINT "swap-ok"
END SUB

SUB Main()
  T_atomic()
END SUB
