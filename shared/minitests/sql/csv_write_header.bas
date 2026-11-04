' CSV scrivibile: header=0 (file senza riga di intestazione) — colonne c0,c1; INSERT/UPDATE/DELETE + persistenza.
' EXPECT: row  2  BOB|row  3  carol|persist= 2
IMPORT SQL
IMPORT IO

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
    MKDIR GMBWORK$
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS(GMBWORK$ + "/nh.csv") THEN KILL GMBWORK$ + "/nh.csv"
    ON ERROR GOTO 0
    OPEN GMBWORK$ + "/nh.csv" FOR OUTPUT AS #1
    PRINT #1, "1,alice"
    PRINT #1, "2,bob"
    CLOSE #1
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE VIRTUAL TABLE t USING csv(name='work/nh.csv', header=0)"
    SQL EXEC M, "INSERT INTO t VALUES (3,'carol')"
    SQL EXEC M, "UPDATE t SET c1='BOB' WHERE c0=2"
    SQL EXEC M, "DELETE FROM t WHERE c0=1"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT c0, c1 FROM t ORDER BY c0"
    DO WHILE SQL.FETCH(rs)
        PRINT "row "; rs("c0"); " "; rs("c1")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
    ' riapre il file: senza header la prima riga non è intestazione → conta le righe reali
    SQL OPEN ":memory:" ALIAS M2
    SQL EXEC M2, "CREATE VIRTUAL TABLE t2 USING csv(name='work/nh.csv', header=0)"
    SQL QUERY M2, rs, "SELECT COUNT(*) AS n FROM t2"
    IF SQL.FETCH(rs) THEN
        PRINT "persist="; rs("n")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M2
END SUB
