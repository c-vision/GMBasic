' MODE(x): valore piu' frequente; a parita' (stesso conteggio) il PIU' PICCOLO; NULL saltati; vuoto -> NULL.
' EXPECT: mode= 4|tie= 1|empty=[]
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE TABLE sv(x REAL)"
    SQL EXEC M, "INSERT INTO sv VALUES (2),(4),(4),(4),(5),(5),(7),(9)"
    SQL EXEC M, "CREATE TABLE tie(v REAL)"
    SQL EXEC M, "INSERT INTO tie VALUES (2),(2),(1),(1),(3)"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT MODE(x) AS m FROM sv"
    IF SQL.FETCH(rs) THEN
        PRINT "mode="; rs("m")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT MODE(v) AS m FROM tie"
    IF SQL.FETCH(rs) THEN
        PRINT "tie="; rs("m")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT MODE(x) AS m FROM sv WHERE 0"
    IF SQL.FETCH(rs) THEN
        PRINT "empty=["; rs("m"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
