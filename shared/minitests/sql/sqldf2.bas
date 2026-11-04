' EXPECT: dfschema={"rows":2,"columns":[{"name":"id","type":"int"},{"name":"name","type":"text"}]}|tocsv= 0|fromcsv= 0
IMPORT SQL
IMPORT IO
  CHDIR PROJECTFOLDER()
MKDIR "work"
SQL OPEN ":memory:" ALIAS st
SQL EXEC st, "CREATE TABLE t(id INTEGER, name TEXT)"
SQL EXEC st, "INSERT INTO t VALUES(1,'a')"
SQL EXEC st, "INSERT INTO t VALUES(2,'b')"
DIM rs AS SQLRESULTSET
SQL QUERY st, rs, "SELECT id, name FROM t ORDER BY id"
DIM d AS STRING
d = SQL.DFSCHEMA(rs)
PRINT "dfschema="; d
PRINT "tocsv="; SQL.TOCSV(rs, "work/sqldf2.csv")
PRINT "fromcsv="; SQL.FROMCSV("st", "t2", "work/sqldf2.csv")
SQL CLOSE RESULTSET rs
SQL CLOSE st
