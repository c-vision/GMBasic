' MiniTest ISAM.APPENDMANY (lista letterale di record) e ISAM.LOG (riepilogo WAL).
' LOG: il testo contiene path assoluti/relativi diversi per gamba → si verificano le chiavi stabili.
' EXPECT: n= 3  last= 3|log=-1|log0=-1
IMPORT ISAM
IMPORT IO
TYPE R
  ID AS STRING * 10
  V AS STRING * 20
END TYPE
SUB Main()
  MKDIR "bin"
  KILL "bin/zzlm.isdb"
  ISAM OPEN "bin/zzlm.isdb" OF R ALIAS X INDEXED BY ID
  DIM n AS LONG
  n = ISAM.APPENDMANY([{ID: "A", V: "uno"}, {ID: "B", V: "due"}, {ID: "C", V: "tre"}])
  PRINT "n="; n; " last="; ISAM.LASTREC()
  PRINT "log="; (INSTR(ISAM.LOG("X"), "wal_path:") > 0)
  PRINT "log0="; (INSTR(ISAM.LOG(), "wal_exists:") > 0)
  ISAM CLOSE
  KILL "bin/zzlm.isdb"
END SUB
