' udt_keyword_member.bas — campo UDT con nome-keyword (volume): assegnazione semplice,
' su elemento di ARRAY di UDT e su elemento di LISTA di UDT. Parità sui backend.
' EXPECT: 5   7|6   8   60|9   90
TYPE P
  volume AS DOUBLE
  qty AS DOUBLE
END TYPE
TYPE W
  n AS DOUBLE
  legs AS LIST OF P
END TYPE
SUB Main()
  DIM p AS P
  p.volume = 5
  p.qty = 7
  PRINT p.volume; " "; p.qty
  DIM arr(2) AS P
  arr(0).volume = 6
  arr(1).volume = 8
  arr(0).qty = 60
  PRINT arr(0).volume; " "; arr(1).volume; " "; arr(0).qty
  DIM w AS W
  DIM ll AS LIST OF P
  DIM x AS P
  x.volume = 1
  x.qty = 10
  ll.Add(x)
  w.legs = ll
  w.legs(0).volume = 9
  w.legs(0).qty = 90
  PRINT w.legs(0).volume; " "; w.legs(0).qty
END SUB
