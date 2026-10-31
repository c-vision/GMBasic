' MiniTest §4.1 — DBF RECALL FOR con lambda FN(rec) => cond
' EXPECT: vis= 2
IMPORT DBF
TYPE TD
    ID AS INTEGER
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_rec.dbf"
    KILL p
    DBF CREATE p FROM TYPE TD
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {ID: 1}
    DBF APPEND RECORD {ID: 2}
    DBF APPEND RECORD {ID: 3}
    DBF DELETE FOR ID > 1
    DBF RECALL FOR FN(rec) => ID = 2
    DBF SET DELETED ON
    DIM n AS INTEGER
    n = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        n = n + 1
        DBF SKIP
    LOOP
    PRINT "vis="; STR(n)
    DBF CLOSE DATABASES
END SUB
