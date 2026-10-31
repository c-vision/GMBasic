' MiniTest DBF — SEEK usa l'ordine CONTROLLANTE; senza indice FOUND()=false (Clipper).
' Path ASSOLUTI (/tmp): la CWD differisce fra i backend. Il MiniTest è il proprio oracolo.
' EXPECT: order= 1 rec= 2 noord= 0
IMPORT DBF
IMPORT IO

TYPE TSeek
    ID AS INTEGER
    Nome AS STRING * 10
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_seek_order.dbf"
    KILL p
    DBF CREATE p FROM TYPE TSeek
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {ID: 1, Nome: "A"}
    DBF APPEND RECORD {ID: 2, Nome: "B"}
    DBF APPEND RECORD {ID: 3, Nome: "C"}
    DBF INDEX ON Nome TO "/tmp/gmb_mt_seek_order.ntx"
    DBF SET ORDER TO 1
    DBF SEEK "B"
    DIM f1 AS INTEGER
    DIM r1 AS INTEGER
    f1 = 0
    IF DBF.FOUND() THEN f1 = 1
    r1 = DBF.RECNO()
    DBF SET ORDER TO 0
    DBF SEEK "B"
    DIM f2 AS INTEGER
    f2 = 0
    IF DBF.FOUND() THEN f2 = 1
    PRINT "order="; STR(f1); " rec="; STR(r1); " noord="; STR(f2)
    DBF CLOSE DATABASES
END SUB
