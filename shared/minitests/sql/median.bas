' MEDIAN(x): mediana (media dei due centrali se n pari). NULL/non numerici saltati;
' insieme vuoto -> NULL. Nessun requisito n>=2.
' EXPECT: even= 4.5|even2= 5.5|odd= 7|empty=[]
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE TABLE sv(x REAL)"
    SQL EXEC M, "INSERT INTO sv VALUES (2),(4),(4),(4),(5),(5),(7),(9)"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT ROUND(MEDIAN(x),4) AS m FROM sv"
    IF SQL.FETCH(rs) THEN
        PRINT "even="; rs("m")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT MEDIAN(x) AS m FROM sv WHERE x IN (2,9)"
    IF SQL.FETCH(rs) THEN
        PRINT "even2="; rs("m")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT MEDIAN(x) AS m FROM sv WHERE x=7"
    IF SQL.FETCH(rs) THEN
        PRINT "odd="; rs("m")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT MEDIAN(x) AS m FROM sv WHERE 0"
    IF SQL.FETCH(rs) THEN
        PRINT "empty=["; rs("m"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
