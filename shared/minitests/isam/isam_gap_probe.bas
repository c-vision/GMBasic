' SONDA GAP (non canonica): ISAM su C99Native/VM-C ha buchi aperti, vedi
' Bugs/(open) - ISAM-RDD-Gap-Backend-C.md. Non ha EXPECT: non entra nel corpus.
' Self-contained: il file .isdb viene creato in work/ dal test (nessuna fixture).
' Replica: log in work/, SHIP/RECV su porta 0 (fuori range) per non aprire socket.
' NB: work/ e non work/ — sulle gambe managed (IL/PCode/VM-C) la cwd e' la RADICE del repo.
' (nessun EXPECT: sonda diagnostica)
IMPORT ISAM
IMPORT IO
IMPORT JSON

TYPE R
  ID AS STRING * 10
  V AS STRING * 20
END TYPE

SUB Stato()
  DIM p AS STRING
  p = GMBWORK$ + "/isam_all.isdb"
  KILL p
  ISAM OPEN p OF R ALIAS X INDEXED BY ID

  DIM rec AS R
  rec.ID = "T1" : rec.V = "uno"
  ISAM APPEND X, rec
  rec.ID = "T2" : rec.V = "due"
  ISAM APPEND X, rec

  PRINT "used="; ISAM.USED("X")
  PRINT "selected>0="; (ISAM.SELECTED() > 0)
  PRINT "lastrec="; ISAM.LASTREC()
  PRINT "recsize>0="; (ISAM.RECSIZE() > 0)
  ISAM GO X, TOP
  PRINT "bof="; ISAM.BOF()
  PRINT "eof="; ISAM.EOF()
  PRINT "recno="; ISAM.RECNO()
  PRINT "indexord>0="; (ISAM.INDEXORD() > 0)
  PRINT "indexkey_len>=0="; (LEN(ISAM.INDEXKEY()) >= 0)
  PRINT "filter_empty="; (LEN(ISAM.DBFILTER()) = 0)
  PRINT "relation_empty="; (LEN(ISAM.DBRELATION()) = 0)
  PRINT "deleted="; ISAM.DELETED()
  PRINT "found="; ISAM.FOUND()
  ISAM SEEK X, "T2"
  PRINT "found2="; ISAM.FOUND()
  PRINT "rlock="; (ISAM.RLOCK() <> 0)
  PRINT "flock="; (ISAM.FLOCK() <> 0)
  PRINT "info>0="; (LEN(JSON.STRINGIFY(ISAM.INFO())) > 0)
  PRINT "integrity>0="; (LEN(JSON.STRINGIFY(ISAM.INTEGRITY())) > 0)

  ISAM CLOSE X
END SUB

SUB Replica()
  DIM p AS STRING
  p = GMBWORK$ + "/isam_rpl.isdb"
  KILL p
  ISAM OPEN p OF R ALIAS Y INDEXED BY ID
  DIM rec AS R
  rec.ID = "R1" : rec.V = "repl"
  ISAM APPEND Y, rec

  DIM lp AS STRING
  lp = ISAM.SETREPLICATION("Y", GMBWORK$ + "/isam_rpl.log")
  PRINT "setrepl>0="; (LEN(lp) > 0)
  PRINT "apply>=-1="; (ISAM.APPLYREPLICATION("Y", lp) >= -1)
  PRINT "follow>=-1="; (ISAM.FOLLOWREPLICATION(lp) >= -1)
  PRINT "ship="; ISAM.REPLICATIONSHIP("127.0.0.1", 0)
  PRINT "recv="; ISAM.REPLICATIONRECV(0)
  ISAM CLOSE Y
END SUB

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  Stato()
  Replica()
END SUB
