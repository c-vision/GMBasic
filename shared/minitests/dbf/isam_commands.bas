' MiniTest ISAM — comandi del namespace (emitter C99Native completato).
' EXPECT: lastrec:  3|top: T1|skip: T2|bottom: T3|seek: T2|count:  3
IMPORT ISAM

TYPE R
    ID AS STRING * 10
    V AS STRING * 20
END TYPE

SUB Main()
    DIM rec AS R
    DIM chk AS R
    ON ERROR RESUME NEXT
    KILL "/tmp/gmb_mt_isamcmd.isdb"
    ON ERROR GOTO 0
    ISAM OPEN "/tmp/gmb_mt_isamcmd.isdb" OF R ALIAS X INDEXED BY ID
    rec.ID = "T1" : rec.V = "V1" : ISAM APPEND X, rec
    rec.ID = "T2" : rec.V = "V2" : ISAM APPEND X, rec
    rec.ID = "T3" : rec.V = "V3" : ISAM APPEND X, rec
    PRINT "lastrec: "; ISAM.LASTREC(X)
    ISAM GO X, TOP
    ISAM GET X, chk
    PRINT "top: "; chk.ID
    ISAM SKIP X, 1
    ISAM GET X, chk
    PRINT "skip: "; chk.ID
    ISAM GO X, BOTTOM
    ISAM GET X, chk
    PRINT "bottom: "; chk.ID
    ISAM SEEK X, "T2"
    IF ISAM.FOUND(X) THEN ISAM GET X, chk
    PRINT "seek: "; chk.ID
    ISAM SET ORDER TO 1 ON X
    ISAM SET FILTER TO "ID > 'T1'" ON X
    ISAM CREATE INDEX "by_v" ON X KEY (V) LEN 20
    ISAM DROP INDEX "by_v" ON X
    ISAM DELETE X
    ISAM RECALL X
    ISAM LOCK X
    ISAM UNLOCK X
    ISAM REINDEX X
    PRINT "count: "; ISAM.LASTREC(X)
    ISAM CLOSE X
    KILL "/tmp/gmb_mt_isamcmd.isdb"
END SUB

