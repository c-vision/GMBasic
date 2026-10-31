' MiniTest §4.1 — SET FILTER TO FN(rec) => cond (lambda) + SET FILTER TO ""
' EXPECT: f= 2 all= 3
IMPORT DBF
TYPE TF
    Citta AS STRING * 20
    Saldo AS INTEGER
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_f.dbf"
    KILL p
    DBF CREATE p FROM TYPE TF
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {Citta: "Roma", Saldo: 100}
    DBF APPEND RECORD {Citta: "Milano", Saldo: 200}
    DBF APPEND RECORD {Citta: "Roma", Saldo: 50}
    DBF SET FILTER TO FN(rec) => Citta = "Roma" AND Saldo > 0
    DIM n AS INTEGER
    n = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        n = n + 1
        DBF SKIP
    LOOP
    DBF SET FILTER TO ""
    DIM a AS INTEGER
    a = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        a = a + 1
        DBF SKIP
    LOOP
    PRINT "f="; STR(n); " all="; STR(a)
    DBF CLOSE DATABASES
END SUB
