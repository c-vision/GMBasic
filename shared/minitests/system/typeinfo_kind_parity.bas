' EXPECT: int=DOUBLE|long=DOUBLE|arr=LIST OF DOUBLE
' SYSTEM.TYPEINFO deve dare lo STESSO nome di tipo sui 4 backend (2026-11-04: IL rispondeva
' "QUAD"/"LONG" mentre C99/PCode/VM-C dicono "DOUBLE", perché rappresentano ogni numero come
' double; il nome è di visualizzazione, non il tipo CLR interno).
IMPORT SYSTEM
SUB Main()
  DIM i AS INTEGER
  DIM l AS LONG
  DIM a(2) AS INTEGER
  i = 1 : l = 1
  PRINT "int="; SYSTEM.TYPEINFO(i); "|long="; SYSTEM.TYPEINFO(l);
  PRINT "|arr="; SYSTEM.TYPEINFO(a)
  PRINT ""
END SUB
