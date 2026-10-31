' MiniTest DBF — FOR/WHILE su operazioni FILE-LEVEL: COPY TO, APPEND FROM, SORT TO, TOTAL ON.
' Path ASSOLUTI (/tmp): la CWD differisce fra i backend. Il MiniTest è il proprio oracolo.
' EXPECT: copy= 2 append= 4 sort= 2000 tot= 2000
IMPORT DBF
IMPORT IO
IMPORT CONVERSIONS

TYPE TF
    ID AS INTEGER
    Nome AS STRING * 20
    Citta AS STRING * 20
    Saldo AS CURRENCY
    Attivo AS BIT
END TYPE

SUB Main()
    DIM p AS STRING
    DIM pr AS STRING
    DIM ps AS STRING
    DIM pt AS STRING
    p = "/tmp/gmb_mt_file_ops.dbf"
    pr = "/tmp/gmb_mt_file_ops_roma.dbf"
    ps = "/tmp/gmb_mt_file_ops_sorted.dbf"
    pt = "/tmp/gmb_mt_file_ops_tot.dbf"
    KILL p
    KILL pr
    KILL ps
    KILL pt
    DBF CREATE p FROM TYPE TF
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {ID: 1, Nome: "A", Citta: "Roma", Saldo: 1100, Attivo: TRUE}
    DBF APPEND RECORD {ID: 2, Nome: "B", Citta: "Milano", Saldo: 2000, Attivo: TRUE}
    DBF APPEND RECORD {ID: 3, Nome: "C", Citta: "Roma", Saldo: 50, Attivo: FALSE}
    DBF COPY TO pr FOR Citta = "Roma"
    DBF USE pr ALIAS R EXCLUSIVE NEW
    DIM nCopy AS INTEGER
    nCopy = 0
    DBF GO TOP
    DO WHILE NOT DBF.EOF()
        nCopy = nCopy + 1
        DBF SKIP
    LOOP
    DBF CLOSE DATABASES
    DBF USE p ALIAS D2 EXCLUSIVE NEW
    DBF APPEND FROM pr FOR Saldo > 150
    DIM nApp AS INTEGER
    nApp = DBF.LASTREC()
    DBF CLOSE DATABASES
    DBF USE p ALIAS D3 EXCLUSIVE NEW
    DBF SORT TO ps ON Saldo /D FOR Attivo = TRUE
    DBF USE ps ALIAS S NEW
    DBF GO TOP
    DIM sFirst AS CURRENCY
    sFirst = Saldo
    DBF CLOSE DATABASES
    DBF USE p ALIAS D4 EXCLUSIVE NEW
    DBF INDEX ON Citta TO "/tmp/gmb_mt_file_ops.ntx"
    DBF SET ORDER TO 1
    DBF TOTAL ON Citta TO pt FIELDS Saldo FOR Saldo > 100
    DBF USE pt ALIAS T NEW
    DBF GO TOP
    DIM tSaldo AS CURRENCY
    tSaldo = Saldo
    PRINT "copy="; STR(nCopy); " append="; STR(nApp); " sort="; CONVERSIONS.CSTR(sFirst); " tot="; CONVERSIONS.CSTR(tSaldo)
    DBF CLOSE DATABASES
END SUB
