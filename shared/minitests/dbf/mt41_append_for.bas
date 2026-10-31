' MiniTest §4.1 — APPEND FROM ... FOR <cond> (filtro sul SORGENTE)
' EXPECT: n= 2
IMPORT DBF
TYPE TA
    Saldo AS INTEGER
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_ap_src.dbf"
    DIM q AS STRING
    q = "/tmp/gmb_mt41_ap_dst.dbf"
    KILL p
    KILL q
    DBF CREATE p FROM TYPE TA
    DBF USE p ALIAS S EXCLUSIVE NEW
    DBF APPEND RECORD {Saldo: 100}
    DBF APPEND RECORD {Saldo: 300}
    DBF CLOSE DATABASES
    DBF CREATE q FROM TYPE TA
    DBF USE q ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {Saldo: 999}
    DBF APPEND FROM p FOR Saldo > 150
    DIM n AS INTEGER
    n = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        n = n + 1
        DBF SKIP
    LOOP
    PRINT "n="; STR(n)
    DBF CLOSE DATABASES
END SUB
