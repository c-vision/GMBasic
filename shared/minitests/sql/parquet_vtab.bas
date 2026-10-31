' Parquet virtual table: interroga un file Parquet direttamente in SQL (scan colonnare nativo).
' Fixture: samples/Fixtures/parquet_flat.parquet (snappy; int64/int32/double/float/bool/string).
' EXPECT: row id= 2  name=bob price= 2.25|row id= 3  name=carol price= 3.125|row id= 4  name=dave price= 4|count= 4  sum= 10.88  maxqty= 40|zstd count= 4  sum= 10.88|nested id= 1  tags=["a","b"] prices=[1,2] info={"city":"rome","zip":100} attrs={"k1":"v1"}|nested id= 2  tags=["c"] prices=[3] info={"city":"milan","zip":200} attrs={"k2":"v2","k3":"v3"}
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE VIRTUAL TABLE p USING parquet(name='../../samples/Fixtures/parquet_flat.parquet')"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT id, name, price FROM p WHERE price > 2 ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "row id="; rs("id"); " name="; rs("name"); " price="; rs("price")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT COUNT(*) AS n, ROUND(SUM(price),2) AS s, MAX(qty) AS m FROM p"
    IF SQL.FETCH(rs) THEN
        PRINT "count="; rs("n"); " sum="; rs("s"); " maxqty="; rs("m")
    END IF
    SQL CLOSE RESULTSET rs
    ' Stesso schema/dati, compressione ZSTD (lettore: zstd nativo).
    SQL EXEC M, "CREATE VIRTUAL TABLE z USING parquet(name='../../samples/Fixtures/parquet_zstd.parquet')"
    SQL QUERY M, rs, "SELECT COUNT(*) AS n, ROUND(SUM(price),2) AS s FROM z"
    IF SQL.FETCH(rs) THEN
        PRINT "zstd count="; rs("n"); " sum="; rs("s")
    END IF
    SQL CLOSE RESULTSET rs
    ' Nested (list/struct/map) → JSON (Fase 2): schema annidato assemblato via def/rep levels.
    SQL EXEC M, "CREATE VIRTUAL TABLE n USING parquet(name='../../samples/Fixtures/parquet_nested.parquet')"
    SQL QUERY M, rs, "SELECT id, tags, prices, info, attrs FROM n ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "nested id="; rs("id"); " tags="; rs("tags"); " prices="; rs("prices"); " info="; rs("info"); " attrs="; rs("attrs")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
