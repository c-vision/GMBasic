' udt_list_field.bas — UDT con campo LISTA di UDT: popolamento (variabile lista),
' proprietà Count, accesso elemento t.legs(i).campo — feature di linguaggio UDT.
' EXPECT: t1       n= 2|leg  0  buy    K= 100|leg  1  sell   K= 105
TYPE Leg
  side AS STRING * 6
  strike AS DOUBLE
END TYPE
TYPE Ticket
  name AS STRING * 8
  legs AS LIST OF Leg
END TYPE
SUB Main()
  DIM t AS Ticket
  DIM lst AS LIST OF Leg
  DIM a AS Leg
  DIM b AS Leg
  a.side = "buy"
  a.strike = 100
  b.side = "sell"
  b.strike = 105
  lst.Add(a)
  lst.Add(b)
  t.legs = lst
  t.name = "t1"
  DIM n AS LONG
  n = t.legs.Count
  PRINT t.name; " n="; n
  DIM i AS LONG
  FOR i = 0 TO n - 1
    PRINT " leg "; i; " "; t.legs(i).side; " K="; t.legs(i).strike
  NEXT
END SUB
