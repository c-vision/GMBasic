' MiniTest §4.1 — COPY TO ... FOR <cond>
' EXPECT: n= 2
IMPORT DBF
TYPE TC
    Citta AS STRING * 20
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_cp.dbf"
    KILL p
    KILL "/tmp/gmb_mt41_copy.dbf"
    DBF CREATE p FROM TYPE TC
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {Citta: "Roma"}
    DBF APPEND RECORD {Citta: "Milano"}
    DBF APPEND RECORD {Citta: "Roma"}
    DBF COPY TO "/tmp/gmb_mt41_copy.dbf" FOR Citta = "Roma"
    DBF USE "/tmp/gmb_mt41_copy.dbf" NEW ALIAS C
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
