' MiniTest §4.1 — TOTAL ON ... FIELDS ... FOR (condizione nuda)
' EXPECT: rows= 2 first=Milano/ 200
IMPORT DBF
TYPE TT
    Citta AS STRING * 20
    Saldo AS INTEGER
    Attivo AS INTEGER
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_t.dbf"
    KILL p
    KILL "/tmp/gmb_mt41_tot.dbf"
    DBF CREATE p FROM TYPE TT
    DBF USE p ALIAS C EXCLUSIVE NEW
    DBF APPEND RECORD {Citta: "Roma", Saldo: 100, Attivo: 1}
    DBF APPEND RECORD {Citta: "Roma", Saldo: 50, Attivo: 1}
    DBF APPEND RECORD {Citta: "Milano", Saldo: 200, Attivo: 1}
    DBF APPEND RECORD {Citta: "Roma", Saldo: 999, Attivo: 0}
    DBF INDEX ON Citta TO "/tmp/gmb_mt41_t.ntx"
    DBF SET ORDER TO 1
    DBF TOTAL ON Citta TO "/tmp/gmb_mt41_tot.dbf" FIELDS Saldo FOR Attivo = 1
    DBF USE "/tmp/gmb_mt41_tot.dbf" NEW ALIAS T
    DIM n AS INTEGER
    n = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        n = n + 1
        DBF SKIP
    LOOP
    DBF GO TOP
    PRINT "rows="; STR(n); " first="; Citta; "/"; STR(Saldo)
    DBF CLOSE DATABASES
END SUB
