' SQL aggregati statistici (gap vs Polars/DuckDB/PostgreSQL): VAR_POP/VAR_SAMP/VARIANCE,
' STDDEV_POP/STDDEV_SAMP/STDDEV e MEDIAN — UDF SQLite registrate su ogni connessione (parità 4 backend).
' Media, dispersione e mediana in una sola query, senza portare i dati in BASIC.
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "work"
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("work/stats.db") THEN KILL "work/stats.db"
    RMDIR "work", TRUE
    ON ERROR GOTO 0
    MKDIR "work"

    SQL OPEN "work/stats.db" ALIAS T
    SQL EXEC T, "CREATE TABLE prices(p REAL, sector TEXT)"
    SQL EXEC T, "INSERT INTO prices VALUES (2,'a'),(4,'a'),(4,'a'),(4,'b'),(5,'b'),(5,'b'),(7,'a'),(9,'b')"

    DIM rs AS SQLRESULTSET
    ' Statistiche globali (ROUND per output stabile).
    SQL QUERY T, rs, "SELECT ROUND(VAR_POP(p),4) AS var_pop, ROUND(VAR_SAMP(p),4) AS var_samp, ROUND(STDDEV_POP(p),4) AS std_pop, ROUND(STDDEV_SAMP(p),4) AS std_samp, ROUND(MEDIAN(p),4) AS median FROM prices"
    IF SQL.FETCH(rs) THEN
        PRINT "globale: var_pop="; rs("var_pop"); " var_samp="; rs("var_samp")
        PRINT "globale: std_pop="; rs("std_pop"); " std_samp="; rs("std_samp"); " median="; rs("median")
    END IF
    SQL CLOSE RESULTSET rs

    ' Statistiche per gruppo (sector): la stessa query funziona con GROUP BY.
    SQL QUERY T, rs, "SELECT sector, ROUND(MEDIAN(p),4) AS median, ROUND(STDDEV_POP(p),4) AS std FROM prices GROUP BY sector ORDER BY sector"
    DO WHILE SQL.FETCH(rs)
        PRINT "sector="; rs("sector"); " median="; rs("median"); " std="; rs("std")
    LOOP
    SQL CLOSE RESULTSET rs

    ' NULL saltati e insieme vuoto -> NULL.
    SQL QUERY T, rs, "SELECT MEDIAN(p) AS m FROM prices WHERE p < 0"
    IF SQL.FETCH(rs) THEN
        PRINT "vuoto: m=["; rs("m"); "]"
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE T
END SUB
