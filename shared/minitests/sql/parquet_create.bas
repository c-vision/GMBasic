' Parquet scrivibile: CREATE da schema con TUTTI i tipi + INSERT/UPDATE/DELETE.
' EXPECT: row  1  alice p= 99  r= 0.5  f= 1  q= 100|row  3  carol p= 15.25  r= 2.5  f= 1  q= 300|after-del= 0|reinsert= 1
IMPORT SQL
IMPORT IO

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
    MKDIR GMBWORK$
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS(GMBWORK$ + "/c.parquet") THEN KILL GMBWORK$ + "/c.parquet"
    ON ERROR GOTO 0
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE VIRTUAL TABLE t USING parquet(name='work/c.parquet', schema='id LONG, name TEXT, price DOUBLE, ratio FLOAT, flag BOOL, qty INT32')"
    SQL EXEC M, "INSERT INTO t VALUES (1,'alice',10.5,0.5,1,100)"
    SQL EXEC M, "INSERT INTO t VALUES (2,'bob',20,1.5,0,200)"
    SQL EXEC M, "INSERT INTO t VALUES (3,'carol',15.25,2.5,1,300)"
    SQL EXEC M, "UPDATE t SET price=99 WHERE name='alice'"
    SQL EXEC M, "DELETE FROM t WHERE id=2"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT id, name, price, ratio, flag, qty FROM t ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "row "; rs("id"); " "; rs("name"); " p="; rs("price"); " r="; rs("ratio"); " f="; rs("flag"); " q="; rs("qty")
    LOOP
    SQL CLOSE RESULTSET rs
    ' DELETE-all (multi-riga) + reinsert
    SQL EXEC M, "DELETE FROM t"
    SQL QUERY M, rs, "SELECT COUNT(*) AS n FROM t"
    IF SQL.FETCH(rs) THEN
        PRINT "after-del="; rs("n")
    END IF
    SQL CLOSE RESULTSET rs
    SQL EXEC M, "INSERT INTO t VALUES (7,'zed',1,1,1,1)"
    SQL QUERY M, rs, "SELECT COUNT(*) AS n FROM t"
    IF SQL.FETCH(rs) THEN
        PRINT "reinsert="; rs("n")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
