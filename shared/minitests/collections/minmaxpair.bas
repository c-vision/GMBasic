' EXPECT: max=b|min=a|count= 3
IMPORT COLLECTIONS
SUB Main()
  ' DICTOFMAXPAIR/MINPAIR: la coppia chiave/valore con valore massimo/minimo (stringa: ordine alfabetico).
  ' Qui numerico: MAXPAIR = chiave del valore piu' alto, MINPAIR = piu' basso.
  DIM d AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
  d.Add("a", 1) : d.Add("b", 3) : d.Add("c", 2)
  PRINT "max="; d.MaxPair()
  PRINT "min="; d.MinPair()
  PRINT "count="; d.Count()
END SUB
