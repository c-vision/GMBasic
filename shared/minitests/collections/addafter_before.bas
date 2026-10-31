' EXPECT: 1|2b|2|3|1|2|3
IMPORT COLLECTIONS
SUB Main()
  ' LISTOFADDAFTER/ADDBEFORE: inserisce DOPO (AddAfter) o PRIMA (AddBefore) dell'indice.
  ' Nella VM: AddAfter(i, v) inserisce DOPO la posizione i (0-based), AddBefore(i, v) prima.
  DIM a AS LIST OF STRING = NEW LIST OF STRING
  a.Add("1") : a.Add("2") : a.Add("3")
  a.AddAfter(0, "2b")
  PRINT a(0) : PRINT a(1) : PRINT a(2) : PRINT a(3)
  DIM b AS LIST OF STRING = NEW LIST OF STRING
  b.Add("2") : b.Add("3")
  b.AddBefore(0, "1")
  PRINT b(0) : PRINT b(1) : PRINT b(2)
END SUB
