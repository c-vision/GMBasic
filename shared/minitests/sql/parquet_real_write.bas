' Parquet REALE scrivibile: copy di pq_multirg (4 row-group, 1000 righe) -> UPDATE/DELETE ->
' il file riscritto (1 row-group) deve conservare i dati; riletto da una SECONDA connessione.
' EXPECT: before n= 1000  sum= 500500|after  n= 999  sum= 500498|reopen n= 999  sum= 500498  z= 1
IMPORT SQL
IMPORT IO

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
    MKDIR GMBWORK$
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS(GMBWORK$ + "/mr.parquet") THEN KILL GMBWORK$ + "/mr.parquet"
    ON ERROR GOTO 0
    COPY "../../samples/Fixtures/pq_multirg.parquet" TO GMBWORK$ + "/mr.parquet"

    SQL OPEN ":memory:" ALIAS A
    SQL EXEC A, "CREATE VIRTUAL TABLE mr USING parquet(name='work/mr.parquet')"
    DIM rs AS SQLRESULTSET
    SQL QUERY A, rs, "SELECT COUNT(*) AS n, SUM(id) AS s FROM mr"
    IF SQL.FETCH(rs) THEN
        PRINT "before n="; rs("n"); " sum="; rs("s")
    END IF
    SQL CLOSE RESULTSET rs
    SQL EXEC A, "UPDATE mr SET cat='z' WHERE id=1"
    SQL EXEC A, "DELETE FROM mr WHERE id=2"
    SQL QUERY A, rs, "SELECT COUNT(*) AS n, SUM(id) AS s FROM mr"
    IF SQL.FETCH(rs) THEN
        PRINT "after  n="; rs("n"); " sum="; rs("s")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE A

    ' rilettura da una nuova connessione (file su disco riscritto)
    SQL OPEN ":memory:" ALIAS B
    SQL EXEC B, "CREATE VIRTUAL TABLE mr2 USING parquet(name='work/mr.parquet')"
    SQL QUERY B, rs, "SELECT COUNT(*) AS n, SUM(id) AS s, SUM(CASE WHEN cat='z' THEN 1 ELSE 0 END) AS nz FROM mr2"
    IF SQL.FETCH(rs) THEN
        PRINT "reopen n="; rs("n"); " sum="; rs("s"); " z="; rs("nz")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE B
END SUB
