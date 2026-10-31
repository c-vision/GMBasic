' CSV virtual table: interroga un file CSV direttamente in SQL (niente import), con WHERE/ORDER/aggregati.
' Fixture: samples/Fixtures/csv_scan.csv (header, quote e virgole interne). Colonne tipizzate per
' contenuto (intero/reale/testo): i confronti e gli aggregati sono numerici dove il dato è numerico.
' EXPECT: row id= 2  name=bob, jr price= 20|row id= 3  name=ca"rol price= 15.25|count= 4  vol= 500
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    SQL EXEC M, "CREATE VIRTUAL TABLE t USING csv(name='../../samples/Fixtures/csv_scan.csv')"
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT id, name, price FROM t WHERE price > 12 ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT "row id="; rs("id"); " name="; rs("name"); " price="; rs("price")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL QUERY M, rs, "SELECT COUNT(*) AS n, SUM(vol) AS tv FROM t"
    IF SQL.FETCH(rs) THEN
        PRINT "count="; rs("n"); " vol="; rs("tv")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
