' EXPECT: mem_ok
' MiniTest: SYSTEM.SYS("MEMTOTAL"/"MEMAVAILABLE") — memoria di sistema (API native) su tutti i backend.
IMPORT SYSTEM WITH ALIAS
SUB Main
  DIM t AS DOUBLE
  t = VAL(SYSTEM.SYS("MEMTOTAL"))
  IF t > 0 THEN
    IF VAL(SYSTEM.SYS("MEMAVAILABLE")) > 0 THEN PRINT "mem_ok" ELSE PRINT "mem_bad"
  ELSE
    PRINT "mem_bad"
  END IF
END SUB
