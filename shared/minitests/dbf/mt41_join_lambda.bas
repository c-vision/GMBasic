' MiniTest §4.1 — JOIN WITH ... FOR FN(cli,mov) => Alias.Campo = Alias.Campo
' EXPECT: n= 3
IMPORT DBF
TYPE TC
    ID AS INTEGER
    Nome AS STRING * 10
END TYPE
TYPE TM
    Rif AS INTEGER
    Imp AS INTEGER
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_jc.dbf"
    DIM q AS STRING
    q = "/tmp/gmb_mt41_jm.dbf"
    KILL p
    KILL q
    KILL "/tmp/gmb_mt41_j.dbf"
    DBF CREATE p FROM TYPE TC
    DBF USE p ALIAS Clienti EXCLUSIVE NEW
    DBF APPEND RECORD {ID: 1, Nome: "A"}
    DBF APPEND RECORD {ID: 2, Nome: "B"}
    DBF CLOSE DATABASES
    DBF CREATE q FROM TYPE TM
    DBF USE q ALIAS Movimenti EXCLUSIVE NEW
    DBF APPEND RECORD {Rif: 1, Imp: 10}
    DBF APPEND RECORD {Rif: 1, Imp: 20}
    DBF APPEND RECORD {Rif: 2, Imp: 30}
    DBF USE p ALIAS Clienti EXCLUSIVE NEW
    DBF SELECT Clienti
    DBF JOIN WITH Movimenti TO "/tmp/gmb_mt41_j.dbf" FOR FN(cli, mov) => Clienti.ID = Movimenti.Rif FIELDS Clienti.Nome, Movimenti.Imp
    DBF USE "/tmp/gmb_mt41_j.dbf" NEW ALIAS J
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
