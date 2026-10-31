' SQL CSV scan — interroga un file CSV DIRETTAMENTE in SQL (nessuna importazione), con
' WHERE / ORDER BY / aggregati. La virtual table `csv` è il sorgente unico gmb_csv_vtab.c
' (registrata staticamente sui backend C, caricata come estensione sui backend C#).
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS C
    ' Il path è relativo alla cartella del programma (fixture condivisa).
    SQL EXEC C, "CREATE VIRTUAL TABLE prezzi USING csv(name='../../Fixtures/csv_scan.csv')"

    DIM rs AS SQLRESULTSET
    PRINT "-- righe con prezzo > 12 (ordinati per id) --"
    SQL QUERY C, rs, "SELECT id, name, price, vol FROM prezzi WHERE price > 12 ORDER BY id"
    DO WHILE SQL.FETCH(rs)
        PRINT rs("id"); " | "; rs("name"); " | "; rs("price"); " | "; rs("vol")
    LOOP
    SQL CLOSE RESULTSET rs

    PRINT "-- aggregati sul CSV --"
    SQL QUERY C, rs, "SELECT COUNT(*) AS n, SUM(vol) AS volume, ROUND(AVG(price),2) AS medio, ROUND(MEDIAN(price),2) AS mediana FROM prezzi"
    IF SQL.FETCH(rs) THEN
        PRINT "righe="; rs("n"); " volume="; rs("volume"); " prezzo_medio="; rs("medio"); " mediana="; rs("mediana")
    END IF
    SQL CLOSE RESULTSET rs

    PRINT "-- quantile 90% (VaR) e moda del volume --"
    SQL QUERY C, rs, "SELECT ROUND(QUANTILE(price,0.9),2) AS p90, MODE(vol) AS moda_vol FROM prezzi"
    IF SQL.FETCH(rs) THEN
        PRINT "p90="; rs("p90"); " moda_vol="; rs("moda_vol")
    END IF
    SQL CLOSE RESULTSET rs

    SQL CLOSE C
END SUB
