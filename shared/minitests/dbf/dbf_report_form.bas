' MiniTest DBF: REPORT FORM <form> TO <file> — il form non e' letto (elenco dei campi per record).
' EXPECT: exists=-1|txt=[1 Alfa|2 Beta|]
IMPORT DBF
IMPORT IO

TYPE TCli
  ID AS INTEGER
  Nome AS STRING * 10
END TYPE

SUB Main()
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_rf.dbf"
  KILL p
  KILL "/tmp/gmb_mt_dbf_rf.txt"
  DBF CREATE p FROM TYPE TCli
  DBF USE p ALIAS D EXCLUSIVE NEW
  DBF APPEND RECORD {ID: 1, Nome: "Alfa"}
  DBF APPEND RECORD {ID: 2, Nome: "Beta"}
  DBF REPORT FORM "/tmp/gmb_mt_dbf_rf.frx" TO FILE "/tmp/gmb_mt_dbf_rf.txt"
  PRINT "exists="; IO.FILEEXISTS("/tmp/gmb_mt_dbf_rf.txt")
  PRINT "txt=["; IO.READFILE("/tmp/gmb_mt_dbf_rf.txt"); "]"
  DBF CLOSE DATABASES
  KILL p
  KILL "/tmp/gmb_mt_dbf_rf.txt"
END SUB
