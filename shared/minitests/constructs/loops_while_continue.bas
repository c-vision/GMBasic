' EXPECT: while-max= 10|loopwhile-count= 5|continue-for= 1|continue-do= 5|wend-ok
SUB Main()
    ' WHILE ... WEND con EXIT WHILE
    DIM i = 1
    DIM sum = 0
    WHILE i <= 10
        IF i = 11 THEN EXIT WHILE
        sum = sum + 1
        i = i + 1
    WEND
    PRINT "while-max="; sum

    ' DO ... LOOP WHILE (post-test)
    DIM j = 0
    DO
        j = j + 1
    LOOP WHILE j < 5
    PRINT "loopwhile-count="; j

    ' CONTINUE dentro FOR: salta l'iterazione 3
    DIM k = 0
    DIM seen = 0
    FOR k = 1 TO 5
        IF k = 3 THEN
            seen = 1
            CONTINUE
        END IF
        IF k = 3 THEN seen = 99
    NEXT k
    PRINT "continue-for="; seen

    ' CONTINUE dentro DO ... LOOP WHILE: salta i pari
    DIM m = 0
    DIM acc = 0
    DO
        m = m + 1
        IF m MOD 2 = 0 THEN CONTINUE
        IF m > 9 THEN EXIT DO
        acc = acc + 1
    LOOP WHILE m < 20
    PRINT "continue-do="; acc

    PRINT "wend-ok"
END SUB
