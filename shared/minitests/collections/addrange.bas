' EXPECT: 1|2|3|4|n= 4|vuota= 0
IMPORT COLLECTIONS
SUB Main()
  ' LISTOFADDRANGE: LIST.AddRange(another) — appende tutti gli elementi.
  DIM a AS LIST OF STRING = NEW LIST OF STRING
  a.Add("1") : a.Add("2")
  DIM b AS LIST OF STRING = NEW LIST OF STRING
  b.Add("3") : b.Add("4")
  a.AddRange(b)
  PRINT a(0) : PRINT a(1) : PRINT a(2) : PRINT a(3)
  PRINT "n="; a.Count()
  a.Clear()
  PRINT "vuota="; a.Count()
END SUB
