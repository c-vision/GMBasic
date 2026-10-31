' Parquet REALI (prodotti da pyarrow 25): multi row-group, null, no-dict, page v2, gzip, tipi extra.
' Verifica che il reader legga TUTTI i row-group (non solo il primo) e i NULL reali.
' EXPECT: mr n= 1000  sum= 500500  min= 1  max= 1000|mr a= 250|mr b= 250|mr c= 250|mr d= 250|nu nulls name= 2  val= 2  flag= 2|nu val  1 = 1.5|nu val  4 = 4|enc nodict= 1225  v2= 1225  gzip= 1225|ty  1   10   1|ty  2   20   2|ty  3   30   3|ts match= 1
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS P

    ' multi row-group (4 RG x 250 righe): gli aggregati devono coprire TUTTE le righe
    SQL EXEC P, "CREATE VIRTUAL TABLE mr USING parquet(name='../../samples/Fixtures/pq_multirg.parquet')"
    DIM rs AS SQLRESULTSET
    SQL QUERY P, rs, "SELECT COUNT(*) AS n, SUM(id) AS s, MIN(id) AS lo, MAX(id) AS hi FROM mr"
    IF SQL.FETCH(rs) THEN
        PRINT "mr n="; rs("n"); " sum="; rs("s"); " min="; rs("lo"); " max="; rs("hi")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY P, rs, "SELECT cat, COUNT(*) AS c FROM mr GROUP BY cat ORDER BY cat"
    DO WHILE SQL.FETCH(rs)
        PRINT "mr "; rs("cat"); "="; rs("c")
    LOOP
    SQL CLOSE RESULTSET rs

    ' NULL reali (BYTE_ARRAY/DOUBLE/BOOLEAN nullable)
    SQL EXEC P, "CREATE VIRTUAL TABLE nu USING parquet(name='../../samples/Fixtures/pq_nulls.parquet')"
    SQL QUERY P, rs, "SELECT SUM(CASE WHEN name IS NULL THEN 1 ELSE 0 END) AS nn, SUM(CASE WHEN val IS NULL THEN 1 ELSE 0 END) AS nv, SUM(CASE WHEN flag IS NULL THEN 1 ELSE 0 END) AS nf FROM nu"
    IF SQL.FETCH(rs) THEN
        PRINT "nu nulls name="; rs("nn"); " val="; rs("nv"); " flag="; rs("nf")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY P, rs, "SELECT id, val FROM nu WHERE val IS NOT NULL ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "nu val "; rs("id"); "="; rs("val")
    LOOP
    SQL CLOSE RESULTSET rs

    ' no-dictionary (PLAIN) + data page v2 + gzip
    SQL EXEC P, "CREATE VIRTUAL TABLE nd USING parquet(name='../../samples/Fixtures/pq_nodict.parquet')"
    SQL EXEC P, "CREATE VIRTUAL TABLE v2 USING parquet(name='../../samples/Fixtures/pq_v2.parquet')"
    SQL EXEC P, "CREATE VIRTUAL TABLE gz USING parquet(name='../../samples/Fixtures/pq_gzip.parquet')"
    SQL QUERY P, rs, "SELECT (SELECT SUM(qty) FROM nd) AS a, (SELECT SUM(qty) FROM v2) AS b, (SELECT SUM(qty) FROM gz) AS c"
    IF SQL.FETCH(rs) THEN
        PRINT "enc nodict="; rs("a"); " v2="; rs("b"); " gzip="; rs("c")
    END IF
    SQL CLOSE RESULTSET rs

    ' tipi extra: uint32/uint64 (INT32/INT64), date32; il timestamp int64 grande è verificato
    ' per uguaglianza esatta in SQL (evita la divergenza GENERALE di PRINT su LONG in VM/C)
    SQL EXEC P, "CREATE VIRTUAL TABLE ty USING parquet(name='../../samples/Fixtures/pq_types.parquet')"
    SQL QUERY P, rs, "SELECT u32, u64, d FROM ty ORDER BY u32"
    DO WHILE SQL.FETCH(rs)
        PRINT "ty "; rs("u32"); " "; rs("u64"); " "; rs("d")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL QUERY P, rs, "SELECT SUM(CASE WHEN ts = 1700000001000000 THEN 1 ELSE 0 END) AS m FROM ty"
    IF SQL.FETCH(rs) THEN
        PRINT "ts match="; rs("m")
    END IF
    SQL CLOSE RESULTSET rs

    SQL CLOSE P
END SUB
