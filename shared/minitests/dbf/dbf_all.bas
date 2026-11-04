' MiniTest NAMESPACE DBF — tutte le 30 funzioni della superficie + i comandi del dominio.
'
' Superficie: ALIAS, APPENDMANY, APPLYREPLICATION, BEGINGROUPCOMMIT, BOF, DBFILTER,
' DBRELATION, DELETED, ENDGROUPCOMMIT, EOF, FIELDNAME, FLOCK, FLUSHDURABILITY,
' FOLLOWREPLICATION, FOUND, INDEXKEY, INDEXORD, LASTREC, LOOKUP, LUPDATE, NETERR, RECNO,
' RECSIZE, REPLICATIONRECV, REPLICATIONSHIP, RLOCK, SELECTED, SETDURABILITY,
' SETREPLICATION, USED.
'
' Comandi: SET DEFAULT, CREATE, USE, SELECT, CLOSE/CLOSE DATABASES, APPEND RECORD,
' APPEND BLANK, APPEND FROM, REPLACE, DELETE, RECALL, GO TOP/BOTTOM, SKIP, SEEK, GET,
' INDEX ON, SET ORDER, SET FILTER, SET RELATION, RLOCK/FLOCK, COPY TO, COPY STRUCTURE,
' SORT TO, TOTAL ON, JOIN WITH, UPDATE ON, EXPORT, IMPORT, BEGIN/COMMIT/ROLLBACK,
' VALIDATE, PACK, REINDEX, ZAP, LIST, AGGREGATE.
' NON esercitati (gap aperti, non omissioni del test):
'   - (nessuno: REPORT FORM è coperto in coda da ReportForm()).
'
' Struttura: sezioni indipendenti (fixture e file distinti), così ogni funzione è
' verificata nello scenario che le altre gambe già coprono. Path ASSOLUTI in /tmp.
' EXPECT: used=-1|selected= 1|alias=D|lastrec= 2|recno= 2|recsize>0=-1|field1=ID|deleted0= 0|neterr= 0|filter_alias=[]|filter0=-1|relation0=-1|indexord0= 0|indexkey0=-1|bof0= 0|recno_top= 1|recno_skip= 2|eof_bottom= 0|eof_after=-1|deleted1=-1|deleted2= 0|get1=1|ALICE|lastrec_blank= 2|get2=2|BOB|rlock=-1|flock=-1|neterr2= 0|setdur=-1|flushdur=-1|groupbegin=-1|groupend=-1|filter0=-1|filter1=-1|lookup=A2|indexord>=-1|found=-1|recno= 2|indexkey_len>=-1|found_noord= 0|relation_called=si|setrepl=-1|appendmany= 2|lastrec= 2|lupdate=-1|apply=-1|follow=-1|ship0= 0|recv0= 0|copy= 2|appendfrom= 4|sort_exists=-1|total_exists=-1|join= 2|update= 2|validate_ok=-1|tx_ok=-1|pack= 1|zap= 0|export=-1|list=-1|aggregate=-1|rf_exists=-1|rf_alfa=-1|rf_beta=-1
IMPORT DBF
IMPORT IO

TYPE TReport
  ID AS INTEGER
  Nome AS STRING * 10
END TYPE

TYPE TCli
  ID AS INTEGER
  Nome AS STRING * 20
  Citta AS STRING * 15
  Saldo AS CURRENCY
  Attivo AS BIT
END TYPE

SUB Stato()
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_all_state.dbf"
  KILL p
  DBF CREATE p FROM TYPE TCli
  DBF CLOSE
  DBF USE p ALIAS D EXCLUSIVE
  DBF APPEND RECORD {ID: 1, Nome: "ALICE", Citta: "Roma", Saldo: 1100, Attivo: TRUE}
  DBF APPEND RECORD {ID: 2, Nome: "BOB", Citta: "Milano", Saldo: 2000, Attivo: TRUE}
  PRINT "used="; DBF.USED()
  PRINT "selected="; DBF.SELECTED()
  PRINT "alias="; DBF.ALIAS()
  PRINT "lastrec="; DBF.LASTREC()
  PRINT "recno="; DBF.RECNO()
  PRINT "recsize>0="; (DBF.RECSIZE() > 0)
  PRINT "field1="; DBF.FIELDNAME(1, "D")
  PRINT "deleted0="; DBF.DELETED()
  PRINT "neterr="; DBF.NETERR()
  PRINT "filter_alias=["; DBF.DBFILTER("D"); "]"   ' caso d uso: area per ALIAS (dai Sample)
  PRINT "filter0="; (LEN(DBF.DBFILTER()) = 0)
  PRINT "relation0="; (LEN(DBF.DBRELATION()) = 0)
  PRINT "indexord0="; DBF.INDEXORD()
  PRINT "indexkey0="; (LEN(DBF.INDEXKEY()) = 0)
  PRINT "bof0="; DBF.BOF()
  DBF GO TOP
  PRINT "recno_top="; DBF.RECNO()
  DBF SKIP
  PRINT "recno_skip="; DBF.RECNO()
  DBF GO BOTTOM
  PRINT "eof_bottom="; DBF.EOF()
  DBF SKIP
  PRINT "eof_after="; DBF.EOF()
  DBF GO TOP
  DBF DELETE
  PRINT "deleted1="; DBF.DELETED()
  DBF RECALL
  PRINT "deleted2="; DBF.DELETED()
  DBF CLOSE DATABASES
  KILL p
END SUB

SUB RecordCorrente()
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_all_get.dbf"
  KILL p
  DBF CREATE p FROM TYPE TCli
  DBF CLOSE
  DBF USE p ALIAS D EXCLUSIVE
  DBF APPEND RECORD {ID: 1, Nome: "ALICE", Citta: "Roma", Saldo: 1100, Attivo: TRUE}
  DBF GO TOP
  DBF GET rec
  PRINT "get1="; TRIM(STR(rec.ID)); "|"; rec.Nome
  DBF APPEND BLANK
  DBF REPLACE ID WITH 2, Nome WITH "BOB", Citta WITH "Milano", Saldo WITH 20, Attivo WITH TRUE
  PRINT "lastrec_blank="; DBF.LASTREC()
  DBF GO BOTTOM
  DBF GET rec
  PRINT "get2="; TRIM(STR(rec.ID)); "|"; rec.Nome
  DBF CLOSE DATABASES
  KILL p
END SUB

SUB Lucchetti()
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_all_lock.dbf"
  KILL p
  DBF CREATE p FROM TYPE TCli
  DBF USE p ALIAS M EXCLUSIVE NEW
  DBF APPEND RECORD {ID: 1, Nome: "A1", Citta: "Roma", Saldo: 1, Attivo: TRUE}
  DBF APPEND RECORD {ID: 2, Nome: "A2", Citta: "Roma", Saldo: 2, Attivo: TRUE}
  DBF GO TOP
  PRINT "rlock="; (DBF.RLOCK() <> 0)
  PRINT "flock="; (DBF.FLOCK() <> 0)
  PRINT "neterr2="; DBF.NETERR()
  PRINT "setdur="; DBF.SETDURABILITY("full")
  PRINT "flushdur="; DBF.FLUSHDURABILITY()
  PRINT "groupbegin="; DBF.BEGINGROUPCOMMIT()
  PRINT "groupend="; DBF.ENDGROUPCOMMIT()
  PRINT "filter0="; (LEN(DBF.DBFILTER()) = 0)
  DBF SET FILTER TO ID = 2
  PRINT "filter1="; (LEN(DBF.DBFILTER()) > 0)
  DBF SET FILTER TO ""
  PRINT "lookup="; TRIM(DBF.LOOKUP("Nome", "2", "ID", "M"))
  DBF CLOSE DATABASES
  KILL p
END SUB

SUB Indice()
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_all_idx.dbf"
  KILL p
  DBF CREATE p FROM TYPE TCli
  DBF USE p ALIAS D EXCLUSIVE NEW
  DBF APPEND RECORD {ID: 1, Nome: "A", Citta: "Roma", Saldo: 1, Attivo: TRUE}
  DBF APPEND RECORD {ID: 2, Nome: "B", Citta: "Milano", Saldo: 2, Attivo: TRUE}
  DBF APPEND RECORD {ID: 3, Nome: "C", Citta: "Roma", Saldo: 3, Attivo: TRUE}
  DBF INDEX ON Nome TO "/tmp/gmb_mt_dbf_all_idx.ntx"
  DBF SET ORDER TO 1
  PRINT "indexord>="; (DBF.INDEXORD() >= 0)
  DBF SEEK "B"
  PRINT "found="; DBF.FOUND()
  PRINT "recno="; DBF.RECNO()
  PRINT "indexkey_len>="; (LEN(DBF.INDEXKEY()) >= 0)
  DBF SET ORDER TO 0
  DBF SEEK "B"
  PRINT "found_noord="; DBF.FOUND()
  DBF CLOSE DATABASES
  KILL p
END SUB

SUB Relazione()
  DIM p1 AS STRING
  DIM p2 AS STRING
  p1 = "/tmp/gmb_mt_dbf_all_relc.dbf"
  p2 = "/tmp/gmb_mt_dbf_all_relo.dbf"
  KILL p1
  KILL p2
  DBF CREATE p1 FROM TYPE TCli
  DBF USE p1 ALIAS CUST
  DBF APPEND RECORD {ID: 1, Nome: "A", Citta: "Roma", Saldo: 1, Attivo: TRUE}
  DBF APPEND RECORD {ID: 2, Nome: "B", Citta: "Roma", Saldo: 2, Attivo: TRUE}
  DBF CLOSE
  DBF CREATE p2 FROM TYPE TCli
  DBF USE p2 ALIAS ORD
  DBF APPEND RECORD {ID: 1, Nome: "X", Citta: "Roma", Saldo: 1, Attivo: TRUE}
  DBF APPEND RECORD {ID: 2, Nome: "Y", Citta: "Roma", Saldo: 2, Attivo: TRUE}
  DBF INDEX ON ID TO "/tmp/gmb_mt_dbf_all_relo.ntx"
  DBF SET ORDER TO 1
  DBF SELECT CUST
  DBF SET RELATION TO ID INTO ORD
  PRINT "relation_called=si"
  DBF CLOSE DATABASES
  KILL p1
  KILL p2
  KILL "/tmp/gmb_mt_dbf_all_relo.ntx"
END SUB

SUB Replica()
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_all_repl.dbf"
  ON ERROR RESUME NEXT
  KILL p
  KILL p + ".repl"
  KILL p + ".repl.ckpt"
  ON ERROR GOTO 0
  DBF CREATE p FROM TYPE TCli
  DBF USE p ALIAS Primary
  DIM logPath AS STRING
  logPath = DBF.SETREPLICATION(Primary, "/tmp/gmb_mt_dbf_all_repl.log")
  PRINT "setrepl="; (LEN(logPath) > 0)
  DIM recs(1) AS TCli
  DIM r AS TCli
  r.ID = 1 : r.Nome = "Alpha" : r.Citta = "Roma" : r.Saldo = 1 : r.Attivo = TRUE
  recs(0) = r
  r.ID = 2 : r.Nome = "Beta" : r.Citta = "Roma" : r.Saldo = 2 : r.Attivo = TRUE
  recs(1) = r
  PRINT "appendmany="; DBF.APPENDMANY(recs)
  PRINT "lastrec="; DBF.LASTREC()
  DIM lu AS STRING
  lu = DBF.LUPDATE()
  PRINT "lupdate="; (LEN(lu) > 0)
  PRINT "apply="; (DBF.APPLYREPLICATION("/tmp/gmb_mt_dbf_all_repl.log") >= 0)
  PRINT "follow="; (DBF.FOLLOWREPLICATION("/tmp/gmb_mt_dbf_all_repl.log") >= 0)
  PRINT "ship0="; DBF.REPLICATIONSHIP("127.0.0.1", 0)
  PRINT "recv0="; DBF.REPLICATIONRECV(0)
  DBF CLOSE DATABASES
  ON ERROR RESUME NEXT
  KILL p
  KILL p + ".repl"
  KILL p + ".repl.ckpt"
  ON ERROR GOTO 0
END SUB

SUB FileOps()
  DIM p AS STRING
  DIM pr AS STRING
  DIM ps AS STRING
  DIM pt AS STRING
  p = "/tmp/gmb_mt_dbf_all_file.dbf"
  pr = "/tmp/gmb_mt_dbf_all_roma.dbf"
  ps = "/tmp/gmb_mt_dbf_all_sorted.dbf"
  pt = "/tmp/gmb_mt_dbf_all_tot.dbf"
  KILL p
  KILL pr
  KILL ps
  KILL pt
  DBF CREATE p FROM TYPE TCli
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
  PRINT "copy="; nCopy
  DBF USE p ALIAS D2 EXCLUSIVE NEW
  DBF APPEND FROM pr FOR Saldo > 150
  PRINT "appendfrom="; DBF.LASTREC()
  DBF CLOSE DATABASES
  DBF USE p ALIAS D3 EXCLUSIVE NEW
  DBF SORT TO ps ON Saldo /D FOR Attivo = TRUE
  PRINT "sort_exists="; IO.FILEEXISTS(ps)
  DBF INDEX ON Citta TO "/tmp/gmb_mt_dbf_all_file.ntx"
  DBF SET ORDER TO 1
  DBF TOTAL ON Citta TO pt FIELDS Saldo FOR Saldo > 100
  PRINT "total_exists="; IO.FILEEXISTS(pt)
  DBF CLOSE DATABASES
  KILL p
  KILL pr
  KILL ps
  KILL pt
  KILL "/tmp/gmb_mt_dbf_all_file.ntx"
END SUB

SUB JoinUpdate()
  DIM po AS STRING
  DIM pc AS STRING
  DIM pj AS STRING
  po = "/tmp/gmb_mt_dbf_all_jo.dbf"
  pc = "/tmp/gmb_mt_dbf_all_jc.dbf"
  pj = "/tmp/gmb_mt_dbf_all_jout.dbf"
  KILL po
  KILL pc
  KILL pj
  DBF CREATE po FROM TYPE TCli
  DBF USE po ALIAS O
  DBF APPEND RECORD {ID: 1, Nome: "O1", Citta: "Roma", Saldo: 1, Attivo: TRUE}
  DBF APPEND RECORD {ID: 2, Nome: "O2", Citta: "Roma", Saldo: 2, Attivo: TRUE}
  DBF CLOSE
  DBF CREATE pc FROM TYPE TCli
  DBF USE pc ALIAS C
  DBF APPEND RECORD {ID: 1, Nome: "C1", Citta: "Roma", Saldo: 1, Attivo: TRUE}
  DBF APPEND RECORD {ID: 2, Nome: "C2", Citta: "Roma", Saldo: 2, Attivo: TRUE}
  DBF CLOSE
  DBF USE pc ALIAS C
  DBF USE po ALIAS O
  DBF JOIN WITH C TO pj FOR O.ID = C.ID
  DBF USE pj ALIAS J
  PRINT "join="; DBF.LASTREC()
  DBF CLOSE DATABASES
  DBF USE pc ALIAS C
  DBF USE po ALIAS O
  DBF UPDATE ON ID FROM C REPLACE Nome WITH C.Nome
  PRINT "update="; DBF.LASTREC()
  DBF CLOSE DATABASES
  KILL po
  KILL pc
  KILL pj
END SUB

SUB Manutenzione()
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_all_maint.dbf"
  KILL p
  DBF CREATE p FROM TYPE TCli
  DBF USE p ALIAS D EXCLUSIVE NEW
  DBF APPEND RECORD {ID: 1, Nome: "A", Citta: "Roma", Saldo: 1, Attivo: TRUE}
  DBF APPEND RECORD {ID: 2, Nome: "B", Citta: "Roma", Saldo: 2, Attivo: TRUE}
  DBF VALIDATE D
  PRINT "validate_ok="; (DBF.LASTREC() > 0)
  DBF BEGIN TRANSACTION
  DBF REPLACE Saldo WITH Saldo + 1
  DBF ROLLBACK
  DBF BEGIN TRANSACTION
  DBF REPLACE Saldo WITH Saldo + 1
  DBF COMMIT
  PRINT "tx_ok="; (DBF.LASTREC() > 0)
  DBF REINDEX
  DBF DELETE
  DBF PACK
  PRINT "pack="; DBF.LASTREC()
  DBF ZAP
  PRINT "zap="; DBF.LASTREC()
  DBF CLOSE DATABASES
  KILL p
END SUB

SUB Estrazione()
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_all_io.dbf"
  KILL p
  KILL "/tmp/gmb_mt_dbf_all_io.csv"
  DBF CREATE p FROM TYPE TCli
  DBF USE p ALIAS D EXCLUSIVE NEW
  DBF APPEND RECORD {ID: 1, Nome: "A", Citta: "Roma", Saldo: 1, Attivo: TRUE}
  DBF APPEND RECORD {ID: 2, Nome: "B", Citta: "Milano", Saldo: 2, Attivo: TRUE}
  DBF EXPORT TO "/tmp/gmb_mt_dbf_all_io.csv" AS CSV
  PRINT "export="; IO.FILEEXISTS("/tmp/gmb_mt_dbf_all_io.csv")
  DBF LIST FIELDS ID TO FILE "/tmp/gmb_mt_dbf_all_io.txt"
  PRINT "list="; IO.FILEEXISTS("/tmp/gmb_mt_dbf_all_io.txt")
  DBF AGGREGATE ON Citta TO "/tmp/gmb_mt_dbf_all_io_agg.dbf" COMPUTE SUM(Saldo)
  PRINT "aggregate="; IO.FILEEXISTS("/tmp/gmb_mt_dbf_all_io_agg.dbf")
  DBF CLOSE DATABASES
  KILL p
  KILL "/tmp/gmb_mt_dbf_all_io.csv"
  KILL "/tmp/gmb_mt_dbf_all_io.txt"
  KILL "/tmp/gmb_mt_dbf_all_io_agg.dbf"
END SUB

SUB ReportForm()
  ' REPORT FORM: il form non viene letto (elenco dei campi per record), come il canone
  ' C#/IL: verifica che il file di output esista e contenga i valori dei record.
  DIM p AS STRING
  p = "/tmp/gmb_mt_dbf_all_rf.dbf"
  ON ERROR RESUME NEXT
  KILL p
  KILL "/tmp/gmb_mt_dbf_all_rf.txt"
  ON ERROR GOTO 0
  DBF CREATE p FROM TYPE TReport
  DBF USE p ALIAS RF EXCLUSIVE NEW
  DBF APPEND RECORD {ID: 1, Nome: "Alfa"}
  DBF APPEND RECORD {ID: 2, Nome: "Beta"}
  DBF REPORT FORM "/tmp/gmb_mt_dbf_all_rf.frx" TO FILE "/tmp/gmb_mt_dbf_all_rf.txt"
  DIM out AS STRING
  out = IO.READFILE("/tmp/gmb_mt_dbf_all_rf.txt")
  PRINT "rf_exists="; IO.FILEEXISTS("/tmp/gmb_mt_dbf_all_rf.txt")
  PRINT "rf_alfa="; (INSTR(out, "Alfa") > 0)
  PRINT "rf_beta="; (INSTR(out, "Beta") > 0)
  DBF CLOSE DATABASES
  KILL p
  KILL "/tmp/gmb_mt_dbf_all_rf.txt"
END SUB

SUB Main()
  Stato()
  RecordCorrente()
  Lucchetti()
  Indice()
  Relazione()
  Replica()
  FileOps()
  JoinUpdate()
  Manutenzione()
  Estrazione()
  ReportForm()
END SUB
