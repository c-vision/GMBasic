' ATOMIC.SWAP: ritorna il valore precedente (parita' Go atomic.SwapInt64).
IMPORT CONCURRENCY
IMPORT ATOMIC

SUB Main()
  DIM c AS ATOMIC
  DIM prev AS LONG
  ATOMIC.STORE(c, 42)
  prev = ATOMIC.SWAP(c, 7)
  IF prev = 42 AND ATOMIC.FETCHVALUE(c) = 7 THEN
    PRINT "ATOMIC-SWAP-OK"
  ELSE
    PRINT "FAIL:prev="; prev
  END IF
END SUB
