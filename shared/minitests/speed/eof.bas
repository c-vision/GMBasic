' MiniTest SPEED — `SPEED.EOF(alias)`.
' Dopo GO TOP il cursore è sul record → EOF falso; dopo SKIP oltre l'ultimo → EOF vero.
' Prima SPEED.EOF cadeva sul ramo FILE (handle) → sempre vero (IL: "Handle file non valido").
' EXPECT: eof-top: no|eof-skip: si
IMPORT IO
IMPORT SPEED

TYPE SRow
  ID AS LONG
  Name AS STRING * 32
END TYPE

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR RESUME NEXT
  RMDIR GMBWORK$ + "/eof_db", TRUE
  ON ERROR GOTO 0
  MKDIR GMBWORK$
  SPEED OPEN GMBWORK$ + "/eof_db" ALIAS MioDb
  SPEED USE "Items" FROM MioDb OF SRow ALIAS T INDEXED BY ID, Name

  DIM rec AS SRow
  rec.ID = 1 : rec.Name = "Alpha" : SPEED PUT T, rec
  rec.ID = 2 : rec.Name = "Beta"  : SPEED PUT T, rec
  rec.ID = 3 : rec.Name = "Gamma" : SPEED PUT T, rec

  SPEED GO T, TOP
  IF SPEED.EOF(T) THEN PRINT "eof-top: si" ELSE PRINT "eof-top: no"

  SPEED SKIP T
  SPEED SKIP T
  SPEED SKIP T
  IF SPEED.EOF(T) THEN PRINT "eof-skip: si" ELSE PRINT "eof-skip: no"
END SUB
