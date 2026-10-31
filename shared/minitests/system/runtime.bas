' EXPECT: numcpu-ok|lbound= 0|sizeof= 8|tab-ok|ngor-ok|gmp-ok|strptr-ok|varptr-ok
IMPORT SYSTEM
SUB Main()
  IF SYSTEM.NUMCPU() > 0 THEN PRINT "numcpu-ok"
  DIM a(4)
  PRINT "lbound="; SYSTEM.LBOUND(a)
  PRINT "sizeof="; SYSTEM.SIZEOF(1.0)
  IF LEN(SYSTEM.TAB(4)) >= 0 THEN PRINT "tab-ok"
  IF SYSTEM.NUMGOROUTINE() >= 0 THEN PRINT "ngor-ok"
  IF SYSTEM.GOMAXPROCS() >= 0 THEN PRINT "gmp-ok"
  DIM s$ = "x"
  IF SYSTEM.STRPTR(s$) <> 0 THEN PRINT "strptr-ok"
  DIM v = 5
  IF SYSTEM.VARPTR(v) <> 0 THEN PRINT "varptr-ok"
END SUB
