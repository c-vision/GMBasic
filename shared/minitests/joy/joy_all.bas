' MiniTest NAMESPACE JOY — tutte le funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: ports-ok|avail-ok|name-ok|mask-ok
IMPORT JOY

SUB T0()
  DIM p = JOY.PORTS()
  IF p >= 0 THEN PRINT "ports-ok"
  IF JOY.AVAILABLE(0) = 0 OR JOY.AVAILABLE(0) = -1 THEN PRINT "avail-ok"
  DIM n$ = JOY.NAME(0)
  IF LEN(n$) >= 0 THEN PRINT "name-ok"
  IF JOY.MASK(0) >= 0 THEN PRINT "mask-ok"
END SUB

SUB Main()
  T0()
END SUB
