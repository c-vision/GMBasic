' MiniTest MEMORY — parametri e semantica di STORE / LOAD / SEARCH / LIST / DELETE.
' Chiave dedicata e cleanup: lo store è condiviso nel processo, le asserzioni usano
' INSTR sulla chiave (mai conteggi globali).
' EXPECT: store=-1|load=valore uno|miss=[]|search=-1|nomatch=[]|list=-1|overwrite=-1|load2=valore due|delete=-1|delete2= 0|emptykey= 0
IMPORT MEMORY
SUB Main()
  MEMORY.DELETE("mt:mem:key")
  PRINT "store="; MEMORY.STORE("mt:mem:key", "valore uno", 0)
  PRINT "load="; MEMORY.LOAD("mt:mem:key")
  PRINT "miss=["; MEMORY.LOAD("mt:mem:assente"); "]"
  PRINT "search="; (INSTR(MEMORY.SEARCH("mt:mem:key"), "mt:mem:key") > 0)
  PRINT "nomatch=["; MEMORY.SEARCH("mt:mem:zzz"); "]"
  PRINT "list="; (INSTR(MEMORY.LIST(), "mt:mem:key") > 0)
  PRINT "overwrite="; MEMORY.STORE("mt:mem:key", "valore due", 1)
  PRINT "load2="; MEMORY.LOAD("mt:mem:key")
  PRINT "delete="; MEMORY.DELETE("mt:mem:key")
  PRINT "delete2="; MEMORY.DELETE("mt:mem:key")
  PRINT "emptykey="; MEMORY.STORE("", "x", 0)
END SUB
