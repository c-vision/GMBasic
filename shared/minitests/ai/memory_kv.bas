' MiniTest: MEMORY.* (STORE/LOAD/SEARCH/LIST/DELETE) — KV legacy su SQLite brain.db.
' Self-cleaning (chiave fissa rimossa prima e dopo) + asserzioni per CONTENUTO via INSTR
' (altri tasti utente/AI possono esistere: mai conteggi). SEARCH cerca sui NOMI chiave.
' Valore booleano canonico GMBASIC = -1 (true) / 0 (false), come BOF/EOF/FOUND/USED.
' EXPECT: stored=-1|loaded=alpha bravo|found= si|listed= si|deleted=-1|gone= si
IMPORT MEMORY
SUB Main()
  ON ERROR RESUME NEXT
  MEMORY.DELETE("gmbtest oracolo")
  ON ERROR GOTO 0
  PRINT "stored="; MEMORY.STORE("gmbtest oracolo", "alpha bravo", 0)
  PRINT "loaded="; MEMORY.LOAD("gmbtest oracolo")
  IF INSTR(MEMORY.SEARCH("gmbtest oracolo"), "gmbtest oracolo") > 0 THEN PRINT "found= si" ELSE PRINT "found= no"
  IF INSTR(MEMORY.LIST(), "gmbtest oracolo") > 0 THEN PRINT "listed= si" ELSE PRINT "listed= no"
  PRINT "deleted="; MEMORY.DELETE("gmbtest oracolo")
  IF MEMORY.LOAD("gmbtest oracolo") = "" THEN PRINT "gone= si" ELSE PRINT "gone= no"
END SUB
