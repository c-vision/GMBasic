' MiniTest DBF: EXPORT / IMPORT CSV e JSON (round-trip, contenuto reale del file).
' Area implicita (corrente) e per alias. Nomi campo in maiuscolo: IL/PCode mantengono il case del TYPE, il motore DBF C usa l'header maiuscolo.
' EXPECT: csv=[ID,NOME|1,ALFA|2,BETA|]|json=[[|{|"ID": 1,|"NOME": "ALFA"|},|{|"ID": 2,|"NOME": "BETA"|}|]]|after_csv= 4|after_json= 6
IMPORT DBF
IMPORT IO

TYPE TCli
  ID AS INTEGER
  Nome AS STRING * 10
END TYPE

SUB Main()
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_exim.dbf"
  KILL p
  KILL "/tmp/gmb_mt_dbf_exim.csv"
  KILL "/tmp/gmb_mt_dbf_exim.json"
  DBF CREATE p FROM TYPE TCli
  DBF USE p ALIAS D EXCLUSIVE NEW
  DBF APPEND RECORD {ID: 1, Nome: "Alfa"}
  DBF APPEND RECORD {ID: 2, Nome: "Beta"}
  DBF EXPORT TO "/tmp/gmb_mt_dbf_exim.csv" AS CSV
  PRINT "csv=["; UCASE(IO.READFILE("/tmp/gmb_mt_dbf_exim.csv")); "]"
  DBF EXPORT TO "/tmp/gmb_mt_dbf_exim.json" AS JSON
  PRINT "json=["; UCASE(IO.READFILE("/tmp/gmb_mt_dbf_exim.json")); "]"
  DBF IMPORT D FROM "/tmp/gmb_mt_dbf_exim.csv" AS CSV
  PRINT "after_csv="; DBF.LASTREC()
  DBF IMPORT D FROM "/tmp/gmb_mt_dbf_exim.json" AS JSON
  PRINT "after_json="; DBF.LASTREC()
  DBF CLOSE DATABASES
  KILL p
  KILL "/tmp/gmb_mt_dbf_exim.csv"
  KILL "/tmp/gmb_mt_dbf_exim.json"
END SUB
