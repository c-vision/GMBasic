' SQL TIMEBUCKET / FIRST_TS / LAST_TS — OHLCV da tick (feature stile Polars group_by_dynamic).
' Crea una tabella tick, la raggruppa in candele di 300s e stampa open/high/low/close/volume.
' Le funzioni temporali sono UDF SQLite registrate su ogni connessione (parità 4 backend).
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "work"
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("work/thread.db") THEN KILL "work/thread.db"
    ON ERROR GOTO 0

    SQL OPEN "work/thread.db" ALIAS TB
    SQL EXEC TB, "CREATE TABLE ticks(ts INTEGER, price INTEGER, vol INTEGER)"
    SQL EXEC TB, "INSERT INTO ticks VALUES (0,10,100),(60,12,200),(120,11,150),(300,20,50),(360,18,75),(420,22,125),(-60,9,10)"

    DIM rs AS SQLRESULTSET
    SQL QUERY TB, rs, "SELECT TIMEBUCKET(ts,300) AS bucket, FIRST_TS(price,ts) AS open, MAX(price) AS high, MIN(price) AS low, LAST_TS(price,ts) AS close, SUM(vol) AS volume FROM ticks GROUP BY bucket ORDER BY bucket"
    PRINT "bucket  open  high  low  close  volume"
    DO WHILE SQL.FETCH(rs)
        PRINT rs("bucket"); "  "; rs("open"); "  "; rs("high"); "  "; rs("low"); "  "; rs("close"); "  "; rs("volume")
    LOOP
    SQL CLOSE RESULTSET rs

    ' Bucket a granularita' diversa (60s) sulla stessa tabella.
    SQL QUERY TB, rs, "SELECT TIMEBUCKET(ts,60) AS b, LAST_TS(price,ts) AS last_price FROM ticks WHERE ts >= 0 GROUP BY b ORDER BY b"
    PRINT "fine-grained close:"
    DO WHILE SQL.FETCH(rs)
        PRINT rs("b"); " -> "; rs("last_price")
    LOOP
    SQL CLOSE RESULTSET rs

    SQL CLOSE TB
END SUB
