' Parquet scrivibile: colonne ANNIDATE (list/struct/map) — UPDATE/INSERT/DELETE con JSON.
' EXPECT: row  1  tags=["x","y","z"] prices=[1,2] info={"city":"rome","zip":100} attrs={"k1":"v1"}|row  3  tags=["p"] prices=[9] info={"city":"turin","zip":7} attrs={"k":"v"}|reopen  1  tags=["x","y","z"] prices=[1,2] info={"city":"rome","zip":100} attrs={"k1":"v1"}|reopen  3  tags=["p"] prices=[9] info={"city":"turin","zip":7} attrs={"k":"v"}
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "work"
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("work/n.parquet") THEN KILL "work/n.parquet"
    ON ERROR GOTO 0
    COPY "../../samples/Fixtures/parquet_nested.parquet" TO "work/n.parquet"
    DIM q$ = CHR(34)
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE VIRTUAL TABLE t USING parquet(name='work/n.parquet')"
    SQL EXEC M, "UPDATE t SET tags='[" + q$ + "x" + q$ + "," + q$ + "y" + q$ + "," + q$ + "z" + q$ + "]' WHERE id=1"
    SQL EXEC M, "UPDATE t SET info='{" + q$ + "city" + q$ + ":" + q$ + "naples" + q$ + "," + q$ + "zip" + q$ + ":999}' WHERE id=2"
    SQL EXEC M, "INSERT INTO t VALUES (3,'[" + q$ + "p" + q$ + "]','[9]','{" + q$ + "city" + q$ + ":" + q$ + "turin" + q$ + "," + q$ + "zip" + q$ + ":7}','{" + q$ + "k" + q$ + ":" + q$ + "v" + q$ + "}')"
    SQL EXEC M, "DELETE FROM t WHERE id=2"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT id, tags, prices, info, attrs FROM t ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "row "; rs("id"); " tags="; rs("tags"); " prices="; rs("prices"); " info="; rs("info"); " attrs="; rs("attrs")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
    ' RIAPERTURA: rilegge il file su disco (non i jrows in memoria) -> intercetta file corrotti
    SQL OPEN ":memory:" ALIAS M2
    SQL EXEC M2, "CREATE VIRTUAL TABLE t2 USING parquet(name='work/n.parquet')"
    SQL QUERY M2, rs, "SELECT id, tags, prices, info, attrs FROM t2 ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "reopen "; rs("id"); " tags="; rs("tags"); " prices="; rs("prices"); " info="; rs("info"); " attrs="; rs("attrs")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE M2
END SUB
