' QUANTILE(x,p): p in [0,1], interpolazione lineare (numpy/pandas). p fuori range/non dato -> NULL;
' insieme vuoto -> NULL.
' EXPECT: q0= 2|q25= 4|q50= 4.5|q100= 9|qbad=[]|qneg=[]|empty=[]
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE TABLE sv(x REAL)"
    SQL EXEC M, "INSERT INTO sv VALUES (2),(4),(4),(4),(5),(5),(7),(9)"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT QUANTILE(x,0.0) AS q0, QUANTILE(x,0.25) AS q25, QUANTILE(x,0.5) AS q50, QUANTILE(x,1.0) AS q100, QUANTILE(x,1.5) AS qbad, QUANTILE(x,-0.1) AS qneg FROM sv"
    IF SQL.FETCH(rs) THEN
        PRINT "q0="; rs("q0")
        PRINT "q25="; rs("q25")
        PRINT "q50="; rs("q50")
        PRINT "q100="; rs("q100")
        PRINT "qbad=["; rs("qbad"); "]"
        PRINT "qneg=["; rs("qneg"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT QUANTILE(x,0.5) AS e FROM sv WHERE 0"
    IF SQL.FETCH(rs) THEN
        PRINT "empty=["; rs("e"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
