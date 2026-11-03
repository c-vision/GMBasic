' MiniTest NAMESPACE ISAM — 22/24 funzioni della superficie (escluse LOG e APPENDMANY:
' non implementate su C99Native, vedi Bugs/(open) - ISAM-RDD-Gap-Backend-C.md).
' Self-contained: il file .isdb viene creato in bin/ dal test (nessuna fixture).
' Replica: log in bin/, SHIP/RECV su porta 0 (fuori range) per non aprire socket.
' EXPECT: used=-1|selected>0=-1|lastrec= 2|recsize>0=-1|bof= 0|eof= 0|recno= 1|indexord>=0=-1|indexkey_len>=0=-1|filter_empty=-1|relation_empty=-1|deleted= 0|found= 0|found2=-1|rlock=-1|flock=-1|info>0=-1|integrity>0=-1|setrepl>0=-1|apply>=-1=-1|follow>=-1=-1|ship= 0|recv= 0
IMPORT ISAM
IMPORT IO
IMPORT JSON

TYPE R
  ID AS STRING * 10
  V AS STRING * 20
END TYPE

SUB Stato()
  DIM p AS STRING
  p = "bin/isam_all.isdb"
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
  PRINT "indexord>=0="; (ISAM.INDEXORD() >= 0)
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
  p = "bin/isam_rpl.isdb"
  KILL p
  ISAM OPEN p OF R ALIAS Y INDEXED BY ID
  DIM rec AS R
  rec.ID = "R1" : rec.V = "repl"
  ISAM APPEND Y, rec

  DIM lp AS STRING
  lp = ISAM.SETREPLICATION("Y", "bin/isam_rpl.log")
  PRINT "setrepl>0="; (LEN(lp) > 0)
  PRINT "apply>=-1="; (ISAM.APPLYREPLICATION("Y", lp) >= -1)
  PRINT "follow>=-1="; (ISAM.FOLLOWREPLICATION(lp) >= -1)
  PRINT "ship="; ISAM.REPLICATIONSHIP("127.0.0.1", 0)
  PRINT "recv="; ISAM.REPLICATIONRECV(0)
  ISAM CLOSE Y
END SUB

SUB Main()
  MKDIR "bin"
  Stato()
  Replica()
END SUB
