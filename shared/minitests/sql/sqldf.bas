' EXPECT: dfschema={"rows":2,"columns":[{"name":"id","type":"int"},{"name":"name","type":"text"}]}|tocsv= 0|fromcsv= 0
' G10 — SQL DataFrame: DESCRIBE + TOCSV + FROMCSV (round-trip CSV).
IMPORT SQL
IMPORT IO
MKDIR "work"
SQL OPEN ":memory:" ALIAS st
SQL EXEC st, "CREATE TABLE t(id INTEGER, name TEXT)"
SQL EXEC st, "INSERT INTO t VALUES(1,'a')"
SQL EXEC st, "INSERT INTO t VALUES(2,'b')"
DIM rs AS SQLRESULTSET
SQL QUERY st, rs, "SELECT id, name FROM t ORDER BY id"
PRINT "dfschema="; SQL.DFSCHEMA(rs)
PRINT "tocsv="; SQL.TOCSV(rs, "work/sqldf.csv")
SQL CLOSE RESULTSET rs
PRINT "fromcsv="; SQL.FROMCSV("st", "t2", "work/sqldf.csv")
SQL CLOSE st
