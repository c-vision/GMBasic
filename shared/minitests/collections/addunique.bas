' EXPECT: n= 2|1|2|1|2|3
IMPORT COLLECTIONS
SUB Main()
  ' LISTOFADDUNIQUE: LIST.AddUnique(x) — aggiunge solo se assente (set-like).
  DIM a AS LIST OF STRING = NEW LIST OF STRING
  a.AddUnique("1") : a.AddUnique("2") : a.AddUnique("1")
  PRINT "n="; a.Count()
  PRINT a(0) : PRINT a(1)
  DIM c AS LIST OF STRING = NEW LIST OF STRING
  c.AddUnique("1") : c.AddUnique("2") : c.AddUnique("3")
  PRINT c(0) : PRINT c(1) : PRINT c(2)
END SUB
