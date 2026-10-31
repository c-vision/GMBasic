' MiniTest §4.1 — SORT ... ON <campo> /D FOR <cond>
' EXPECT: first= 2000 n= 3
IMPORT DBF
TYPE TS
    Nome AS STRING * 10
    Saldo AS INTEGER
    Attivo AS INTEGER
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_s.dbf"
    KILL p
    KILL "/tmp/gmb_mt41_sorted.dbf"
    DBF CREATE p FROM TYPE TS
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {Nome: "A", Saldo: 100, Attivo: 1}
    DBF APPEND RECORD {Nome: "B", Saldo: 2000, Attivo: 1}
    DBF APPEND RECORD {Nome: "C", Saldo: 50, Attivo: 1}
    DBF APPEND RECORD {Nome: "D", Saldo: 999, Attivo: 0}
    DBF SORT TO "/tmp/gmb_mt41_sorted.dbf" ON Saldo /D FOR Attivo = 1
    DBF USE "/tmp/gmb_mt41_sorted.dbf" NEW ALIAS S
    DIM n AS INTEGER
    n = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        n = n + 1
        DBF SKIP
    LOOP
    DBF GO TOP
    PRINT "first="; STR(Saldo); " n="; STR(n)
    DBF CLOSE DATABASES
END SUB
