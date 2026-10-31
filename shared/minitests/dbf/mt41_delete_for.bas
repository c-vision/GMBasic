' MiniTest §4.1 — DBF DELETE FOR (per-record) + SET DELETED ON/OFF
' EXPECT: vis= 1 all= 3
IMPORT DBF
TYPE TD
    ID AS INTEGER
    Saldo AS INTEGER
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_del.dbf"
    KILL p
    DBF CREATE p FROM TYPE TD
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {ID: 1, Saldo: 100}
    DBF APPEND RECORD {ID: 2, Saldo: 300}
    DBF APPEND RECORD {ID: 3, Saldo: 500}
    DBF DELETE FOR Saldo > 200
    DBF SET DELETED ON
    DIM n AS INTEGER
    n = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        n = n + 1
        DBF SKIP
    LOOP
    DBF SET DELETED OFF
    DIM a AS INTEGER
    a = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        a = a + 1
        DBF SKIP
    LOOP
    PRINT "vis="; STR(n); " all="; STR(a)
    DBF CLOSE DATABASES
END SUB
