' Parquet — AGGIUNTA di record a file esistenti (flat e nested), poi RIAPERTURA del file.
' Copre: lista a più elementi, struct con campo mancante, map vuota.
' EXPECT: flat n= 1001  sum= 502500|nest n= 4|flat reopen n= 1001  sum= 502500|nest  1  tags=["a","b"] prices=[1,2] info={"city":"rome","zip":100} attrs={"k1":"v1"}|nest  2  tags=["c"] prices=[3] info={"city":"milan","zip":200} attrs={"k2":"v2","k3":"v3"}|nest  50  tags=["n"] prices=[5] info={"city":"x","zip":null} attrs={"k":"v"}|nest  51  tags=["n2","n3"] prices=[6,7] info={"city":"y","zip":null} attrs={}
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "bin"
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("bin/app.parquet") THEN KILL "bin/app.parquet"
    IF IO.FILEEXISTS("bin/appn.parquet") THEN KILL "bin/appn.parquet"
    ON ERROR GOTO 0
    COPY "../../samples/Fixtures/pq_multirg.parquet" TO "bin/app.parquet"
    COPY "../../samples/Fixtures/parquet_nested.parquet" TO "bin/appn.parquet"

    SQL OPEN ":memory:" ALIAS A
    ' flat: aggiungi un record
    SQL EXEC A, "CREATE VIRTUAL TABLE f USING parquet(name='bin/app.parquet')"
    SQL EXEC A, "INSERT INTO f VALUES (2000,'zz',7,9.99)"
    ' nested: due record (lista 1 elem; lista 2 elem + struct senza zip + map vuota)
    SQL EXEC A, "CREATE VIRTUAL TABLE g USING parquet(name='bin/appn.parquet')"
    DIM q AS STRING
    q = CHR(34)
    SQL EXEC A, "INSERT INTO g VALUES (50,'[" + q + "n" + q + "]','[5]','{" + q + "city" + q + ":" + q + "x" + q + "}','{" + q + "k" + q + ":" + q + "v" + q + "}')"
    SQL EXEC A, "INSERT INTO g VALUES (51,'[" + q + "n2" + q + "," + q + "n3" + q + "]','[6,7]','{" + q + "city" + q + ":" + q + "y" + q + "}','{}')"
    DIM rs AS SQLRESULTSET
    SQL QUERY A, rs, "SELECT COUNT(*) AS n, SUM(id) AS s FROM f"
    IF SQL.FETCH(rs) THEN
        PRINT "flat n="; rs("n"); " sum="; rs("s")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY A, rs, "SELECT COUNT(*) AS n FROM g"
    IF SQL.FETCH(rs) THEN
        PRINT "nest n="; rs("n")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE A

    ' RIAPERTURA: rilegge dal disco (valida il file scritto, non i jrows in memoria)
    SQL OPEN ":memory:" ALIAS B
    SQL EXEC B, "CREATE VIRTUAL TABLE f2 USING parquet(name='bin/app.parquet')"
    SQL QUERY B, rs, "SELECT COUNT(*) AS n, SUM(id) AS s FROM f2"
    IF SQL.FETCH(rs) THEN
        PRINT "flat reopen n="; rs("n"); " sum="; rs("s")
    END IF
    SQL CLOSE RESULTSET rs
    SQL EXEC B, "CREATE VIRTUAL TABLE g2 USING parquet(name='bin/appn.parquet')"
    SQL QUERY B, rs, "SELECT id, tags, prices, info, attrs FROM g2 ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "nest "; rs("id"); " tags="; rs("tags"); " prices="; rs("prices"); " info="; rs("info"); " attrs="; rs("attrs")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE B
END SUB
