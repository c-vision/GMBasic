' STDDEV_POP(x): deviazione standard di popolazione. NULL/non numerici saltati; insieme vuoto -> NULL;
' VAR_SAMP/VARIANCE/STDDEV_SAMP/STDDEV richiedono n>=2 (altrimenti NULL).
' EXPECT: val= 2|empty=[]|one=[ 0 ]
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE TABLE sv(x REAL)"
    SQL EXEC M, "INSERT INTO sv VALUES (2),(4),(4),(4),(5),(5),(7),(9)"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT ROUND(STDDEV_POP(x),4) AS v, STDDEV_POP(x) AS raw FROM sv"
    IF SQL.FETCH(rs) THEN
        PRINT "val="; rs("v")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT STDDEV_POP(x) AS e FROM sv WHERE 0"
    IF SQL.FETCH(rs) THEN
        PRINT "empty=["; rs("e"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT STDDEV_POP(x) AS o FROM sv WHERE x=2"
    IF SQL.FETCH(rs) THEN
        PRINT "one=["; rs("o"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
