' MiniTest ISAM TRANSAZIONI — BEGIN / PUT / COMMIT persiste la modifica.
' Verifica: una PUT dentro BEGIN TRANSACTION ... COMMIT resta dopo il commit.
' EXPECT: before= 100|after= 200|assert= OK
IMPORT ISAM

TYPE R
    ID AS STRING * 10
    V AS STRING * 20
END TYPE

SUB Main()
    DIM rec AS R
    DIM chk AS R
    ON ERROR RESUME NEXT
    KILL "/tmp/gmb_mt_isam_commit.isdb"
    ON ERROR GOTO 0
    ISAM OPEN "/tmp/gmb_mt_isam_commit.isdb" OF R ALIAS X INDEXED BY ID
    rec.ID = "K1" : rec.V = "100" : ISAM APPEND X, rec

    ISAM SEEK X, "K1"
    IF ISAM.FOUND(X) THEN ISAM GET X, chk
    PRINT "before= "; chk.V

    ISAM BEGIN TRANSACTION
    chk.V = "200"
    ISAM PUT X, chk
    ISAM COMMIT

    ISAM SEEK X, "K1"
    IF ISAM.FOUND(X) THEN ISAM GET X, chk
    PRINT "after= "; chk.V
    IF chk.V = "200" THEN
        PRINT "assert= OK"
    ELSE
        PRINT "assert= FAIL"
    END IF
    ISAM CLOSE X
END SUB
