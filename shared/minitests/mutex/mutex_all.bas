' MiniTest NAMESPACE MUTEX — tutte le funzioni della superficie.
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: t1=-1|t2= 0
IMPORT CONCURRENCY
IMPORT MUTEX

SUB T_mutex()
  DIM mu AS MUTEX
  PRINT "t1="; MUTEX.TRYLOCK(mu)
  PRINT "t2="; MUTEX.TRYLOCK(mu)
  MUTEX UNLOCK mu
END SUB

SUB Main()
  T_mutex()
END SUB
