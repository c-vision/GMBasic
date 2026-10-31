' CSV scrivibile: INSERT/UPDATE/DELETE riscrivono il file CSV. Stesso comportamento su ogni sorgente.
' EXPECT: row  1  alice  99|row  3  carol  15.25|row  4  dave  8
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "bin"
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("bin/w.csv") THEN KILL "bin/w.csv"
    ON ERROR GOTO 0
    OPEN "bin/w.csv" FOR OUTPUT AS #1
    PRINT #1, "id,name,price"
    PRINT #1, "1,alice,10.5"
    PRINT #1, "2,bob,20"
    PRINT #1, "3,carol,15.25"
    CLOSE #1

    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE VIRTUAL TABLE t USING csv(name='bin/w.csv')"
    SQL EXEC M, "INSERT INTO t VALUES (4,'dave','8')"
    SQL EXEC M, "UPDATE t SET price='99' WHERE name='alice'"
    SQL EXEC M, "DELETE FROM t WHERE id=2"

    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT id, name, price FROM t ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "row "; rs("id"); " "; rs("name"); " "; rs("price")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
