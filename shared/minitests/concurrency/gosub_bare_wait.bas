' MiniTest: WAIT nuda attende i GO SUB senza IN (bug 2026-10-01).
' Senza gruppo esplicito, i figli si registrano sul gruppo implicito del task
' che li lancia; la WAIT senza gruppo li attende tutti. I figli con IN restano
' di competenza del loro gruppo (la WAIT nuda NON li attende). Senza figli,
' la WAIT nuda è no-op (non si blocca).
' EXPECT: barewait-ok
DIM done1 AS INTEGER
DIM done2 AS INTEGER
DIM done3 AS INTEGER
DIM slowflag AS INTEGER

SUB W1()
    SLEEP 400
    done1 = -1
END SUB

SUB W2()
    SLEEP 400
    done2 = -1
END SUB

SUB W3()
    SLEEP 400
    done3 = -1
END SUB

SUB SlowIn()
    SLEEP 1500
    slowflag = -1
END SUB

SUB Main()
    DIM ok AS INTEGER = -1
    GO SUB W1()
    GO SUB W2()
    GO SUB W3()
    WAIT
    IF done1 <> -1 THEN ok = 0
    IF done2 <> -1 THEN ok = 0
    IF done3 <> -1 THEN ok = 0
    DIM w AS WAITGROUP
    GO SUB SlowIn() IN w
    WAIT
    IF slowflag <> 0 THEN ok = 0
    WAIT w
    IF slowflag <> -1 THEN ok = 0
    WAIT
    IF ok THEN PRINT "barewait-ok"
END SUB
