' MiniTest SPEED/MOUSE — chiusura gap C99Native (durability/group-commit/info + click).
' EXPECT: setdur: si|flush: si|begin: si|end: si|info: si|click0: si|area0: si
IMPORT SPEED

TYPE Riga
  ID AS LONG
  Nome AS STRING * 16
END TYPE

SUB Main()
  ' Stato di test IDEMPOTENTE: senza `MKDIR "bin"` l'apertura "bin/gaps_db" fallisce sul
  ' lato C (il C# crea la cartella da sé) e i 4 controlli di durability non stampano nulla
  ' → il gate dava VMC rosso solo quando bin/ era assente. Stessa forma di set_filter.bas.
  ON ERROR RESUME NEXT
  RMDIR "bin/gaps_db", TRUE
  ON ERROR GOTO 0
  MKDIR "bin"
  SPEED OPEN "bin/gaps_db" ALIAS G
  SPEED USE "T" FROM G OF Riga ALIAS T INDEXED BY ID
  IF UPPER(SPEED.SETDURABILITY("FULL")) = "FULL" THEN PRINT "setdur: si"
  IF SPEED.FLUSHDURABILITY() THEN PRINT "flush: si"
  IF SPEED.BEGINGROUPCOMMIT() THEN PRINT "begin: si"
  IF SPEED.ENDGROUPCOMMIT() THEN PRINT "end: si"
  IF LEN(SPEED.INFO()) > 0 THEN PRINT "info: si"
  IF NOT MOUSE.CLICK() THEN PRINT "click0: si"
  IF NOT MOUSE.CLICK(100, 100, 200, 200) THEN PRINT "area0: si"
  SPEED CLOSE DATABASES
END SUB
