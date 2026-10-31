' SQL Parquet scan — interroga un file Parquet DIRETTAMENTE in SQL (reader Parquet nativo:
' Thrift metadata + pagine + dictionary/RLE + compressione none/snappy/zstd/gzip), con WHERE/
' ORDER BY/aggregati e le UDF (MEDIAN, QUANTILE…). Nessuna importazione in tabella.
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS P
    SQL EXEC P, "CREATE VIRTUAL TABLE ticks USING parquet(name='../../Fixtures/parquet_flat.parquet')"

    DIM rs AS SQLRESULTSET
    PRINT "-- righe con price > 2 --"
    SQL QUERY P, rs, "SELECT id, name, price, qty FROM ticks WHERE price > 2 ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT rs("id"); " | "; rs("name"); " | "; rs("price"); " | "; rs("qty")
    LOOP
    SQL CLOSE RESULTSET rs

    PRINT "-- aggregati (anche UDF SQL) --"
    SQL QUERY P, rs, "SELECT COUNT(*) AS n, ROUND(SUM(price),3) AS tot, ROUND(MEDIAN(price),3) AS med, ROUND(QUANTILE(price,0.25),3) AS q1, MAX(qty) AS maxqty FROM ticks"
    IF SQL.FETCH(rs) THEN
        PRINT "righe="; rs("n"); " somma="; rs("tot"); " mediana="; rs("med"); " q1="; rs("q1"); " maxqty="; rs("maxqty")
    END IF
    SQL CLOSE RESULTSET rs

    PRINT "-- colonne annidate (list/struct/map -> JSON) --"
    SQL EXEC P, "CREATE VIRTUAL TABLE nested USING parquet(name='../../Fixtures/parquet_nested.parquet')"
    SQL QUERY P, rs, "SELECT id, info, tags FROM nested ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT rs("id"); " info="; rs("info"); " tags="; rs("tags")
    LOOP
    SQL CLOSE RESULTSET rs

    PRINT "-- file REALI (pyarrow): 4 row-group, page v2, gzip, NULL --"
    SQL EXEC P, "CREATE VIRTUAL TABLE mr USING parquet(name='../../Fixtures/pq_multirg.parquet')"
    SQL QUERY P, rs, "SELECT COUNT(*) AS n, SUM(id) AS s, MIN(id) AS lo, MAX(id) AS hi FROM mr"
    IF SQL.FETCH(rs) THEN
        PRINT "multirg righe="; rs("n"); " somma="; rs("s"); " min="; rs("lo"); " max="; rs("hi")
    END IF
    SQL CLOSE RESULTSET rs
    SQL EXEC P, "CREATE VIRTUAL TABLE nu USING parquet(name='../../Fixtures/pq_nulls.parquet')"
    SQL QUERY P, rs, "SELECT COUNT(*) AS n, SUM(CASE WHEN name IS NULL THEN 1 ELSE 0 END) AS nn FROM nu"
    IF SQL.FETCH(rs) THEN
        PRINT "nulls righe="; rs("n"); " name_null="; rs("nn")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE P
END SUB
