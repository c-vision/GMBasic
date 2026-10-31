' SQL TIMEBUCKET/FIRST_TS/LAST_TS — OHLCV a bucket di 300s (oracolo per la parità 4 backend).
' O=first, H=max, L=min, C=last, V=sum; bucket negativo incluso (floor, non troncamento).
' EXPECT: b=-300  o= 9  h= 9  l= 9  c= 9  v= 10|b= 0  o= 10  h= 12  l= 10  c= 11  v= 450|b= 300  o= 20  h= 22  l= 18  c= 22  v= 250
IMPORT SQL
IMPORT IO

SUB Main()
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("work/test_tb.db") THEN KILL "work/test_tb.db"
    IF IO.FILEEXISTS("work/test_tb.db-wal") THEN KILL "work/test_tb.db-wal"
    IF IO.FILEEXISTS("work/test_tb.db-shm") THEN KILL "work/test_tb.db-shm"
    RMDIR "work", TRUE
    ON ERROR GOTO 0
    MKDIR "work"
    SQL OPEN "work/test_tb.db" ALIAS TB
    SQL EXEC TB, "CREATE TABLE ticks(ts INTEGER, price INTEGER, vol INTEGER)"
    SQL EXEC TB, "INSERT INTO ticks VALUES (0,10,100),(60,12,200),(120,11,150),(300,20,50),(360,18,75),(420,22,125),(-60,9,10)"
    DIM rs AS SQLRESULTSET
    SQL QUERY TB, rs, "SELECT TIMEBUCKET(ts,300) AS b, FIRST_TS(price,ts) AS o, MAX(price) AS h, MIN(price) AS l, LAST_TS(price,ts) AS c, SUM(vol) AS v FROM ticks GROUP BY b ORDER BY b"
    DO WHILE SQL.FETCH(rs)
        PRINT "b="; rs("b"); " o="; rs("o"); " h="; rs("h"); " l="; rs("l"); " c="; rs("c"); " v="; rs("v")
    LOOP
    SQL CLOSE RESULTSET rs
    SQL CLOSE TB
END SUB
