' MiniTest §4.1 — INDEX ON chiave composta + SET ORDER TO "<nome>" senza estensione
' EXPECT: first=Firenze/Alfa
IMPORT DBF
TYPE TC
    Citta AS STRING * 20
    Nome AS STRING * 20
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_ci.dbf"
    KILL p
    KILL "/tmp/gmb_mt41_ci.ntx"
    DBF CREATE p FROM TYPE TC
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {Citta: "Roma", Nome: "Zeta"}
    DBF APPEND RECORD {Citta: "Firenze", Nome: "Alfa"}
    DBF APPEND RECORD {Citta: "Roma", Nome: "Alfa"}
    DBF INDEX ON Citta + Nome TO "/tmp/gmb_mt41_ci.ntx"
    DBF SET ORDER TO "/tmp/gmb_mt41_ci"
    DBF GO TOP
    PRINT "first="; Citta; "/"; Nome
    DBF CLOSE DATABASES
END SUB
