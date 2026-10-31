' CORR(y,x): correlazione di Pearson. y = 2x+1 (correlazione perfetta). ROUND(...,4) per output stabile.
' NULL/non numerici saltati; requisiti n: COVAR_POP n>=1, COVAR_SAMP/CORR n>=2; insieme vuoto -> NULL.
' EXPECT: val= 1|empty=[]
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE TABLE xy(x REAL, y REAL); INSERT INTO xy VALUES (2,5),(4,9),(4,9),(4,9),(5,11),(5,11),(7,15),(9,19)"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT ROUND(CORR(y,x),4) AS v FROM xy"
    IF SQL.FETCH(rs) THEN
        PRINT "val="; rs("v")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT CORR(y,x) AS e FROM xy WHERE 0"
    IF SQL.FETCH(rs) THEN
        PRINT "empty=["; rs("e"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
