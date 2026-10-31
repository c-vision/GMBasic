' Regressione: SQL dentro una goroutine (GO SUB) deve funzionare — l'alias di un comando SQL,
' se e' un NOME, e' la STRINGA del nome (parita' col compilatore bytecode), visibile a ogni
' procedura/goroutine. Prima da fuori Main falliva (IL "db not open", C99/PCode no-op).
' EXPECT: w-start|w-end|n= 2  sum= 3
IMPORT SQL

SUB W()
    PRINT "w-start"
    SQL EXEC C, "INSERT INTO t VALUES (1,'a')"
    SQL EXEC C, "INSERT INTO t VALUES (2,'b')"
    PRINT "w-end"
END SUB

SUB Main()
    SQL OPEN ":memory:" ALIAS C
    SQL EXEC C, "CREATE TABLE t (id INT, n TEXT)"
    GO SUB W()
    SLEEP 30
    WAIT
    DIM rs AS SQLRESULTSET
    SQL QUERY C, rs, "SELECT COUNT(*) AS n, SUM(id) AS s FROM t"
    IF SQL.FETCH(rs) THEN
        PRINT "n="; rs("n"); " sum="; rs("s")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE C
END SUB
