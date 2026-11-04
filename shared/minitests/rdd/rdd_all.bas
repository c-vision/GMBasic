' MiniTest NAMESPACE RDD — tutte le 22 funzioni della superficie.
' Ordine: prima i COMANDI (GO/REPLACE), poi le funzioni, così non restano lock aperti.
' EXPECT: replaced= 999|deleted=-1|recalled= 0|recno= 1|bof= 0|eof= 0|found= 0|valtype=DBF|f1= 1|f2=ALICE|fs= 0|old=[]|buffer=-1|update=-1|after_update= 777|buffer2=-1|revert=-1|after_revert= 777|setdeleted=-1|ctod=2026-01-15|today_len= 10|float= 12|cur= 49|rlock=-1|flock=-1
IMPORT RDD
IMPORT IO

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  MKDIR GMBWORK$
  COPY "../../samples/Fixtures/rdd_mini.dbf" TO GMBWORK$ + "/rdd_all.dbf"
  RDD USE GMBWORK$ + "/rdd_all.dbf" ALIAS R

  ' --- comandi ---
  RDD GO TOP
  RDD REPLACE Saldo WITH 999
  PRINT "replaced="; RDD.GETFIELD(1, 3)
  RDD DELETE
  PRINT "deleted="; RDD.DELETED()
  RDD RECALL
  PRINT "recalled="; RDD.DELETED()

  ' --- navigazione e campi ---
  PRINT "recno="; RDD.RECNO()
  PRINT "bof="; RDD.BOF()
  PRINT "eof="; RDD.EOF()
  PRINT "found="; RDD.FOUND()
  PRINT "valtype="; RDD.VALTYPE(1)
  PRINT "f1="; RDD.GETFIELD(1, 1)
  PRINT "f2="; RDD.GETFIELD(1, 2)
  PRINT "fs="; RDD.GETFLDSTATE(1, 1)
  PRINT "old=["; RDD.OLDVAL(1, 1); "]"

  ' --- buffer ottimistico (con modifica reale, così UPDATE è significativo) ---
  PRINT "buffer="; (RDD.BUFFER(1, 3) <> 0)
  RDD SET FIELD 1, 3, "777"
  PRINT "update="; RDD.TABLEUPDATE(1)
  PRINT "after_update="; RDD.GETFIELD(1, 3)
  PRINT "buffer2="; (RDD.BUFFER(1, 3) <> 0)
  RDD SET FIELD 1, 3, "888"
  PRINT "revert="; (RDD.TABLEREVERT(1) <> 0)
  PRINT "after_revert="; RDD.GETFIELD(1, 3)

  ' --- SETDELETED: valore BIT non uniforme fra le gambe (1 / -1) → si verifica l'effetto ---
  PRINT "setdeleted="; (RDD.SETDELETED(1, 0) <> 0)

  ' --- tipi e date ---
  PRINT "ctod="; RDD.DTOC(RDD.CTOD("2026-01-15"))
  PRINT "today_len="; LEN(RDD.DTOC(RDD.TODAY()))
  PRINT "float="; RDD.FLOAT(12)
  PRINT "cur="; RDD.CURVAL(49)

  ' --- lock (uno alla volta, con UNLOCK immediato) ---
  PRINT "rlock="; (RDD.RLOCK(1) <> 0)
  RDD UNLOCK 1
  PRINT "flock="; (RDD.FLOCK(1) <> 0)
  RDD UNLOCK 1

  RDD CLOSE 1
END SUB
