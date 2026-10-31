' EXPECT: exec-ok=-1|lastid= 1|changes= 1
IMPORT SQL
SUB Main()
  ' Isolamento: il DB SQLite è in bin/ e il rowid di AUTOINCREMENT persiste tra run.
  ' KILL dei file DB (RMDIR da solo non basta su PCode) rende lastid deterministico (1).
  ON ERROR RESUME NEXT
  IF IO.FILEEXISTS("bin/test.db") THEN KILL "bin/test.db"
  IF IO.FILEEXISTS("bin/test.db-wal") THEN KILL "bin/test.db-wal"
  IF IO.FILEEXISTS("bin/test.db-shm") THEN KILL "bin/test.db-shm"
  RMDIR "bin", TRUE
  ON ERROR GOTO 0
  MKDIR "bin"
  SQL OPEN "bin/test.db" ALIAS AppDb
  DIM r1 = SQL.EXEC("AppDb", "CREATE TABLE IF NOT EXISTS t (id INTEGER PRIMARY KEY, v INTEGER)")
  DIM r2 = SQL.EXEC("AppDb", "INSERT INTO t (v) VALUES (10)")
  PRINT "exec-ok="; r2 >= 0
  PRINT "lastid="; SQL.LASTINSERTROWID("AppDb")
  PRINT "changes="; SQL.CHANGES("AppDb")
  SQL CLOSE AppDb
END SUB
