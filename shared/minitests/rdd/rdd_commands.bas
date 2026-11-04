' MiniTest RDD — comandi e funzioni su area DBF (fixture rdd_mini.dbf copiata in work/):
' BOF, BUFFER, CTOD, CURVAL, DELETE, DTOC, EOF, FLOAT, FLOCK, FOUND, GETFIELD,
' GETFLDSTATE, OLDVAL, RECALL, RECNO, REPLACE, RLOCK, SETDELETED, TABLEREVERT,
' TABLEUPDATE, TODAY, VALTYPE. DELETE/RECALL/REPLACE sono COMANDI (sintassi legacy):
' il file di lavoro è una copia della fixture in work/, la fixture resta read-only.
' EXPECT: recno= 1|bof= 0|eof= 0|found= 0|valtype=DBF|f1= 1|f2=ALICE|fs= 0|old=|rlock=-1|flock=-1|buffer=-1|update=-1|revert= 0|setdeleted=-1|ctod=2026-01-15|today_len= 10|float= 12.5|cur= 49.99|deleted=-1|recalled= 0|replaced= 999.5
IMPORT RDD
IMPORT IO

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  COPY "../../samples/Fixtures/rdd_mini.dbf" TO GMBWORK$ + "/rdd_all.dbf"
  RDD USE GMBWORK$ + "/rdd_all.dbf" ALIAS R

  PRINT "recno="; RDD.RECNO()
  PRINT "bof="; RDD.BOF()
  PRINT "eof="; RDD.EOF()
  PRINT "found="; RDD.FOUND()
  PRINT "valtype="; RDD.VALTYPE(1)
  PRINT "f1="; RDD.GETFIELD(1, 1)
  PRINT "f2="; RDD.GETFIELD(1, 2)
  PRINT "fs="; RDD.GETFLDSTATE(1, 1)
  PRINT "old="; RDD.OLDVAL(1, 1)
  PRINT "rlock="; (RDD.RLOCK(1) <> 0)
  PRINT "flock="; (RDD.FLOCK(1) <> 0)
  RDD UNLOCK 1
  PRINT "buffer="; RDD.BUFFER(1, 3)
  PRINT "update="; RDD.TABLEUPDATE(1)
  PRINT "revert="; RDD.TABLEREVERT(1)
  PRINT "setdeleted="; RDD.SETDELETED(1, 0)
  PRINT "ctod="; RDD.DTOC(RDD.CTOD("2026-01-15"))
  PRINT "today_len="; LEN(RDD.DTOC(RDD.TODAY()))
  PRINT "float="; RDD.FLOAT(12.5)
  PRINT "cur="; RDD.CURVAL(49.99)

  RDD GO TOP
  RDD DELETE
  PRINT "deleted="; RDD.DELETED()
  RDD RECALL
  PRINT "recalled="; RDD.DELETED()
  RDD REPLACE Saldo WITH 999.5
  PRINT "replaced="; RDD.GETFIELD(1, 3)

  RDD CLOSE 1
  KILL GMBWORK$ + "/rdd_all.dbf"
END SUB
