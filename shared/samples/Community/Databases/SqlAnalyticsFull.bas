' SQL Analytics completo — usa OGNI funzione SQL implementata:
'  temporali: TIMEBUCKET, FIRST_TS, LAST_TS
'  statistiche: VAR_POP, VAR_SAMP, VARIANCE, STDDEV_POP, STDDEV_SAMP, STDDEV, MEDIAN
' Scenario finance: da tick a candele OHLCV + statistiche per bucket in una sola query.
IMPORT SQL
IMPORT IO

SUB Main()
    MKDIR "work"
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("work/analytics.db") THEN KILL "work/analytics.db"
    RMDIR "work", TRUE
    ON ERROR GOTO 0
    MKDIR "work"

    SQL OPEN "work/analytics.db" ALIAS A
    SQL EXEC A, "CREATE TABLE ticks(ts INTEGER, price REAL, vol INTEGER)"
    SQL EXEC A, "INSERT INTO ticks VALUES (0,10,100),(60,12,200),(120,11,150),(300,20,50),(360,18,75),(420,22,125)"

    DIM rs AS SQLRESULTSET
    ' 1) Candele OHLCV: TIMEBUCKET (bucket) + FIRST_TS/LAST_TS (open/close per TEMPO) + MIN/MAX/SUM.
    SQL QUERY A, rs, "SELECT TIMEBUCKET(ts,300) AS bucket, FIRST_TS(price,ts) AS o, MAX(price) AS h, MIN(price) AS l, LAST_TS(price,ts) AS c, SUM(vol) AS v FROM ticks GROUP BY bucket ORDER BY bucket"
    DO WHILE SQL.FETCH(rs)
        PRINT "candle b="; rs("bucket"); " o="; rs("o"); " h="; rs("h"); " l="; rs("l"); " c="; rs("c"); " v="; rs("v")
    LOOP
    SQL CLOSE RESULTSET rs

    ' 2) Statistiche di prezzo per bucket: tutti gli aggregati, ROUND per output stabile.
    SQL QUERY A, rs, "SELECT TIMEBUCKET(ts,300) AS bucket, ROUND(VAR_POP(price),4) AS varpop, ROUND(VAR_SAMP(price),4) AS varsamp, ROUND(VARIANCE(price),4) AS variance, ROUND(STDDEV_POP(price),4) AS stdpop, ROUND(STDDEV_SAMP(price),4) AS stdsamp, ROUND(STDDEV(price),4) AS std, ROUND(MEDIAN(price),4) AS median FROM ticks GROUP BY bucket ORDER BY bucket"
    DO WHILE SQL.FETCH(rs)
        PRINT "stats b="; rs("bucket"); " varpop="; rs("varpop"); " varsamp="; rs("varsamp"); " variance="; rs("variance"); " stdpop="; rs("stdpop"); " stdsamp="; rs("stdsamp"); " std="; rs("std"); " median="; rs("median")
    LOOP
    SQL CLOSE RESULTSET rs

    ' 3) Mediana globale (n pari -> media dei due centrali) e varianza campionaria globale.
    SQL QUERY A, rs, "SELECT ROUND(MEDIAN(price),4) AS med, ROUND(STDDEV(price),4) AS sd FROM ticks"
    IF SQL.FETCH(rs) THEN
        PRINT "global: median="; rs("med"); " stddev="; rs("sd")
    END IF
    SQL CLOSE RESULTSET rs

    ' 4) Statistiche avanzate: MODE, QUANTILE (VaR 5%), CORR e covarianza prezzo/volume.
    SQL QUERY A, rs, "SELECT MODE(vol) AS mvol, ROUND(QUANTILE(price,0.05),4) AS var95, ROUND(QUANTILE(price,0.5),4) AS q50 FROM ticks"
    IF SQL.FETCH(rs) THEN
        PRINT "advanced: mode_vol="; rs("mvol"); " quantile05="; rs("var95"); " quantile50="; rs("q50")
    END IF
    SQL CLOSE RESULTSET rs
    SQL QUERY A, rs, "SELECT ROUND(CORR(price,vol),4) AS corr, ROUND(COVAR_SAMP(price,vol),4) AS covs, ROUND(COVAR_POP(price,vol),4) AS covp FROM ticks"
    IF SQL.FETCH(rs) THEN
        PRINT "advanced: corr="; rs("corr"); " covar_samp="; rs("covs"); " covar_pop="; rs("covp")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE A
END SUB
