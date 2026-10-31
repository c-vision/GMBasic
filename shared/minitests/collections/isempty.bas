' EXPECT: vuota=-1|piena= 0|count= 1|svuotata=-1
IMPORT COLLECTIONS
SUB Main()
  ' LISTOFISEMPTY: LIST.IsEmpty() — 1 se vuota, 0 altrimenti (grafia case-insensitive).
  DIM a AS LIST OF STRING = NEW LIST OF STRING
  PRINT "vuota="; a.IsEmpty()
  a.Add("1")
  PRINT "piena="; a.IsEmpty()
  PRINT "count="; a.Count()
  a.Clear()
  PRINT "svuotata="; a.IsEmpty()
END SUB
