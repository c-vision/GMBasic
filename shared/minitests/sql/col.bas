' EXPECT: 1|2|b
' G10 — SQL.COL: intera colonna → ARRAY.
IMPORT SQL
SUB Main()
  SQL OPEN ":memory:" ALIAS st
  SQL EXEC st, "CREATE TABLE t(id INTEGER, name TEXT)"
  SQL EXEC st, "INSERT INTO t VALUES(1,'a')"
  SQL EXEC st, "INSERT INTO t VALUES(2,'b')"
  DIM rs AS SQLRESULTSET
  SQL QUERY st, rs, "SELECT id, name FROM t ORDER BY id"
  DIM ids() = SQL.COL(rs, "id")
  DIM nm() = SQL.COL(rs, "name")
  PRINT ids(0)
  PRINT ids(1)
  PRINT nm(1)
  SQL CLOSE RESULTSET rs
  SQL CLOSE st
END SUB
