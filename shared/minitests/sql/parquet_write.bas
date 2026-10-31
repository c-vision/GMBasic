' Parquet scrivibile: CREATE (da schema) + INSERT/UPDATE/DELETE riscrivono il file Parquet.
' Stesso comportamento su ogni sorgente (tabelle, csv, parquet).
' EXPECT: row  1  alice  99|row  3  carol  15.25|nested  1  ["x","y"] {"city":"rome","zip":100}
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "bin"
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("bin/pq.parquet") THEN KILL "bin/pq.parquet"
    ON ERROR GOTO 0
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE VIRTUAL TABLE t USING parquet(name='bin/pq.parquet', schema='id LONG, name TEXT, price DOUBLE')"
    SQL EXEC M, "INSERT INTO t VALUES (1,'alice',10.5)"
    SQL EXEC M, "INSERT INTO t VALUES (2,'bob',20)"
    SQL EXEC M, "INSERT INTO t VALUES (3,'carol',15.25)"
    SQL EXEC M, "UPDATE t SET price=99 WHERE name='alice'"
    SQL EXEC M, "DELETE FROM t WHERE id=2"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT id, name, price FROM t ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "row "; rs("id"); " "; rs("name"); " "; rs("price")
    LOOP
    SQL CLOSE RESULTSET rs
    ' Nested (list/struct) scrivibile: copia la fixture e modifica via SQL.
    COPY "../../samples/Fixtures/parquet_nested.parquet" TO "bin/nested.parquet"
    SQL EXEC M, "CREATE VIRTUAL TABLE nt USING parquet(name='bin/nested.parquet')"
    DIM q$ = CHR(34)
    SQL EXEC M, "UPDATE nt SET tags='[" + q$ + "x" + q$ + "," + q$ + "y" + q$ + "]' WHERE id=1"
    SQL EXEC M, "DELETE FROM nt WHERE id=2"
    DIM rs2 AS SQLRESULTSET
    SQL QUERY M, rs2, "SELECT id, tags, info FROM nt ORDER BY id"
    DO WHILE SQL.FETCH(rs2)
        PRINT "nested "; rs2("id"); " "; rs2("tags"); " "; rs2("info")
    LOOP
    SQL CLOSE RESULTSET rs2
    SQL CLOSE M
END SUB
