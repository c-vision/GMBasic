' MiniTest DBF — DBF GET <var> in late-binding: la variabile contiene i campi del record.
' Path ASSOLUTI (/tmp): la CWD differisce fra i backend. Il MiniTest è il proprio oracolo.
' EXPECT: id= 7 nome=Mario
IMPORT DBF
IMPORT IO

TYPE TGet
    ID AS INTEGER
    Nome AS STRING * 20
END TYPE

SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt_get_var.dbf"
    KILL p
    DBF CREATE p FROM TYPE TGet
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {ID: 7, Nome: "Mario"}
    DBF GO TOP
    DBF GET clienteRec
    PRINT "id="; STR(clienteRec.ID); " nome="; clienteRec.Nome
    DBF CLOSE DATABASES
END SUB
