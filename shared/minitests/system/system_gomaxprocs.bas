' MiniTest SYSTEM.GOMAXPROCS: forma a 0 argomenti, valore precedente, valore impostato.
' EXPECT: q0>0=-1|prev>0=-1|cur= 2|cur2= 2
IMPORT SYSTEM

SUB Main()
  DIM prev AS LONG
  PRINT "q0>0="; (SYSTEM.GOMAXPROCS() > 0)
  prev = SYSTEM.GOMAXPROCS(2)
  PRINT "prev>0="; (prev > 0)
  PRINT "cur="; SYSTEM.GOMAXPROCS()
  PRINT "cur2="; SYSTEM.GOMAXPROCS(0)
END SUB
