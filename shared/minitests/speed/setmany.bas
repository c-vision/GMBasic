' MiniTest: SPEED.SETMANY(area, records) — bulk insert da LIST OF record UDT.
' 2 record inseriti → ritorna 2. DB locale in work/ (relativo alla dir del test).
' EXPECT: setmany:  2
IMPORT IO
IMPORT SPEED

TYPE SRow
  ID AS LONG
  Name AS STRING * 32
END TYPE

SUB Main()
  ON ERROR RESUME NEXT
  RMDIR "work/sm_db", TRUE
  MKDIR "work"
  ON ERROR GOTO 0
  SPEED OPEN "work/sm_db" ALIAS SmDb
  SPEED USE "Rows" FROM SmDb OF SRow ALIAS SmTbl INDEXED BY ID
  DIM batch AS LIST OF SRow
  batch = NEW LIST OF SRow {}
  DIM r1 AS SRow
  r1.ID = 1 : r1.Name = "Alpha" : batch.Add(r1)
  DIM r2 AS SRow
  r2.ID = 2 : r2.Name = "Beta" : batch.Add(r2)
  DIM n AS LONG
  n = SPEED.SETMANY(SmTbl, batch)
  PRINT "setmany: "; n
END SUB
