' LAST_TS(x, ts): x della riga con ts MASSIMO; tipo preservato; pari ts -> prima riga;
' righe con ts/x NULL saltate; vuoto -> NULL.
' EXPECT: int= 30|real= 3.5|str=d|empty=[]
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE TABLE t(i INTEGER, r REAL, s TEXT, ts INTEGER)"
    SQL EXEC M, "INSERT INTO t VALUES (10,1.5,'a',1),(20,2.5,'b',2),(5,0.5,'c',1),(30,3.5,'d',3)"
    SQL EXEC M, "INSERT INTO t VALUES (99,9.9,'z',NULL)"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT LAST_TS(i,ts) AS ai, LAST_TS(r,ts) AS ar, LAST_TS(s,ts) AS asx FROM t"
    IF SQL.FETCH(rs) THEN
        PRINT "int="; rs("ai")
        PRINT "real="; rs("ar")
        PRINT "str="; rs("asx")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT LAST_TS(i,ts) AS e FROM t WHERE 0"
    IF SQL.FETCH(rs) THEN
        PRINT "empty=["; rs("e"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
