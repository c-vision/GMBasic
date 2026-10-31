' TIMEBUCKET(ts, secs) = FLOOR(ts/secs)*secs. Parametri e risultati per forma.
' int/int -> int (floor vera anche su ts negativi); misto int/real o real -> real;
' ts non numerico, secs NULL/<=0 -> NULL.
' EXPECT: neg_int=-300|pos_int= 300|exact_int=-300|int_real= 120|real_int= 120|real_real= 120|bad_ts=|zero_secs=|null_secs=
IMPORT SQL

SUB Main()
    SQL OPEN ":memory:" ALIAS M
    DIM rs AS SQLRESULTSET
    SQL QUERY M, rs, "SELECT TIMEBUCKET(-1,300) AS a, TIMEBUCKET(400,300) AS b, TIMEBUCKET(-300,300) AS c, TIMEBUCKET(150,60.0) AS d, TIMEBUCKET(150.0,60) AS e, TIMEBUCKET(150.5,60.0) AS f, TIMEBUCKET('abc',300) AS g, TIMEBUCKET(7,0) AS h, TIMEBUCKET(7,NULL) AS i FROM (SELECT 1)"
    IF SQL.FETCH(rs) THEN
        PRINT "neg_int="; rs("a")
        PRINT "pos_int="; rs("b")
        PRINT "exact_int="; rs("c")
        PRINT "int_real="; rs("d")
        PRINT "real_int="; rs("e")
        PRINT "real_real="; rs("f")
        PRINT "bad_ts="; rs("g")
        PRINT "zero_secs="; rs("h")
        PRINT "null_secs="; rs("i")
    END IF
    SQL CLOSE RESULTSET rs
    SQL CLOSE M
END SUB
