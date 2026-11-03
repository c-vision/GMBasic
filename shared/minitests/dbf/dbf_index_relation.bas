' MiniTest DBF: INDEXORD / INDEXKEY / DBRELATION con valori REALI (non solo >= 0).
' EXPECT: ord0= 0|key0=|ord1= 1|key1=NOME|ord_off= 0|rel0=|rel1=ID|rel_off=
IMPORT DBF
IMPORT IO

TYPE TCli
  ID AS INTEGER
  Nome AS STRING * 10
END TYPE

SUB Main()
  DIM p1 AS STRING
  DIM p2 AS STRING
  p1 = "/tmp/gmb_mt_dbf_ixrel_a.dbf"
  p2 = "/tmp/gmb_mt_dbf_ixrel_b.dbf"
  KILL p1
  KILL p2
  DBF CREATE p1 FROM TYPE TCli
  DBF USE p1 ALIAS CUST EXCLUSIVE NEW
  DBF APPEND RECORD {ID: 1, Nome: "A"}
  DBF APPEND RECORD {ID: 2, Nome: "B"}
  PRINT "ord0="; DBF.INDEXORD()
  PRINT "key0="; DBF.INDEXKEY()
  DBF INDEX ON Nome TO "/tmp/gmb_mt_dbf_ixrel_a.ntx"
  DBF SET ORDER TO 1
  PRINT "ord1="; DBF.INDEXORD()
  PRINT "key1="; UCASE(DBF.INDEXKEY())
  DBF SET ORDER TO 0
  PRINT "ord_off="; DBF.INDEXORD()
  DBF CREATE p2 FROM TYPE TCli
  DBF USE p2 ALIAS ORD EXCLUSIVE NEW
  DBF APPEND RECORD {ID: 1, Nome: "X"}
  DBF INDEX ON ID TO "/tmp/gmb_mt_dbf_ixrel_b.ntx"
  DBF SET ORDER TO 1
  DBF SELECT CUST
  PRINT "rel0="; DBF.DBRELATION()
  DBF SET RELATION TO ID INTO ORD
  PRINT "rel1="; DBF.DBRELATION()
  DBF SET RELATION TO
  PRINT "rel_off="; DBF.DBRELATION()
  DBF CLOSE DATABASES
  KILL p1
  KILL p2
  KILL "/tmp/gmb_mt_dbf_ixrel_a.ntx"
  KILL "/tmp/gmb_mt_dbf_ixrel_b.ntx"
END SUB
