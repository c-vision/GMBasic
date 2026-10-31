' MiniTest DBF — INDEX ON con ESPRESSIONE (Citta + STR(Saldo)) e SET ORDER TO 1.
' Path ASSOLUTI (/tmp): la CWD differisce fra i backend. Il MiniTest è il proprio oracolo.
' EXPECT: first=Milano id= 101
IMPORT DBF
IMPORT IO

TYPE TIx
    ID AS INTEGER
    Nome AS STRING * 20
    Citta AS STRING * 20
    Saldo AS CURRENCY
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_index_expr.dbf"
    KILL p
    DBF CREATE p FROM TYPE TIx
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {ID: 100, Nome: "Export Test", Citta: "Roma", Saldo: 1100}
    DBF APPEND RECORD {ID: 101, Nome: "Export Test 2", Citta: "Milano", Saldo: 2000}
    DBF INDEX ON Citta + STR(Saldo) TO "/tmp/gmb_mt_index_expr.ntx"
    DBF SET ORDER TO 1
    DBF GO TOP
    PRINT "first="; Citta; " id="; STR(ID)
    DBF CLOSE DATABASES
END SUB
