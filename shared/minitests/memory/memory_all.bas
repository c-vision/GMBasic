' MiniTest NAMESPACE MEMORY — tutte le 5 funzioni della superficie:
' STORE, LOAD, SEARCH, LIST, DELETE (KV su brain.db SQLite).
' Self-cleaning: chiave fissa rimossa prima e dopo; le asserzioni usano INSTR sul
' CONTENUTO (altri tasti possono esistere), mai conteggi. BIT canonico: -1 = vero.
' EXPECT: stored=-1|loaded=alpha bravo|found= si|listed= si|deleted=-1|gone= si
IMPORT MEMORY

SUB Main()
  ON ERROR RESUME NEXT
  MEMORY.DELETE("gmbtest namespace")
  ON ERROR GOTO 0

  PRINT "stored="; MEMORY.STORE("gmbtest namespace", "alpha bravo", 0)
  PRINT "loaded="; MEMORY.LOAD("gmbtest namespace")
  IF INSTR(MEMORY.SEARCH("gmbtest namespace"), "gmbtest namespace") > 0 THEN PRINT "found= si" ELSE PRINT "found= no"
  IF INSTR(MEMORY.LIST(), "gmbtest namespace") > 0 THEN PRINT "listed= si" ELSE PRINT "listed= no"
  PRINT "deleted="; MEMORY.DELETE("gmbtest namespace")
  IF MEMORY.LOAD("gmbtest namespace") = "" THEN PRINT "gone= si" ELSE PRINT "gone= no"
END SUB
