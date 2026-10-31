' MiniTest §4.1 — DBF LIST con FOR e TO FILE (file di testo rispetto alla CWD)
' EXPECT: file= 1
IMPORT DBF
IMPORT IO
TYPE TL
    ID AS INTEGER
    Citta AS STRING * 20
END TYPE
SUB Main()
    DIM p AS STRING
    p = "/tmp/gmb_mt41_l.dbf"
    KILL p
    KILL "/tmp/gmb_mt41_list.txt"
    DBF CREATE p FROM TYPE TL
    DBF USE p ALIAS D EXCLUSIVE NEW
    DBF APPEND RECORD {ID: 1, Citta: "Roma"}
    DBF APPEND RECORD {ID: 2, Citta: "Milano"}
    DBF APPEND RECORD {ID: 3, Citta: "Roma"}
    DBF LIST TO FILE "/tmp/gmb_mt41_list.txt" FOR Citta = "Roma"
    IF IO.FileExists("/tmp/gmb_mt41_list.txt") THEN PRINT "file= 1"
    DBF CLOSE DATABASES
END SUB
