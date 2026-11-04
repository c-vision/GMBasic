' SQL aggregati statistici (gap vs Polars/DuckDB): VAR_POP/STDDEV_POP/MEDIAN/VAR_SAMP/STDDEV_SAMP.
' Set classico 2,4,4,4,5,5,7,9: var_pop 4, stddev_pop 2, median 4.5, var_samp 32/7, stddev_samp sqrt(32/7).
' EXPECT: vp= 4|sp= 2|md= 4.5|vs= 4.5714|ss= 2.1381
IMPORT SQL
IMPORT IO

SUB Main()
    ON ERROR RESUME NEXT
    IF IO.FILEEXISTS("work/st.db") THEN KILL "work/st.db"
    RMDIR "work", TRUE
    ON ERROR GOTO 0
    MKDIR "work"
    SQL OPEN "work/st.db" ALIAS S
    SQL EXEC S, "CREATE TABLE sv(x REAL)"
    SQL EXEC S, "INSERT INTO sv VALUES (2),(4),(4),(4),(5),(5),(7),(9)"
    DIM rs AS SQLRESULTSET
    SQL QUERY S, rs, "SELECT ROUND(VAR_POP(x),4) AS vp, ROUND(STDDEV_POP(x),4) AS sp, ROUND(MEDIAN(x),4) AS md, ROUND(VAR_SAMP(x),4) AS vs, ROUND(STDDEV_SAMP(x),4) AS ss FROM sv"
    IF SQL.FETCH(rs) THEN
        PRINT "vp="; rs("vp")
        PRINT "sp="; rs("sp")
        PRINT "md="; rs("md")
        PRINT "vs="; rs("vs")
        PRINT "ss="; rs("ss")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE S
END SUB
