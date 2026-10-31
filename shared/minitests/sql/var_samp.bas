' VAR_SAMP(x): varianza campionaria (divide per n-1). NULL/non numerici saltati; insieme vuoto -> NULL;
' VAR_SAMP/VARIANCE/STDDEV_SAMP/STDDEV richiedono n>=2 (altrimenti NULL).
' EXPECT: val= 4.5714|empty=[]|one=[]
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE TABLE sv(x REAL)"
    SQL EXEC M, "INSERT INTO sv VALUES (2),(4),(4),(4),(5),(5),(7),(9)"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT ROUND(VAR_SAMP(x),4) AS v, VAR_SAMP(x) AS raw FROM sv"
    IF SQL.FETCH(rs) THEN
        PRINT "val="; rs("v")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT VAR_SAMP(x) AS e FROM sv WHERE 0"
    IF SQL.FETCH(rs) THEN
        PRINT "empty=["; rs("e"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT VAR_SAMP(x) AS o FROM sv WHERE x=2"
    IF SQL.FETCH(rs) THEN
        PRINT "one=["; rs("o"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
