' MiniTest: array-campo di CLASSE 2-D di UDT (PUBLIC DIM m(2,2) AS Punto) —
' scrittura/lettura di campo su un elemento 2-D (m(1,2).x / .y).
' EXPECT: u= 12
TYPE Punto
  x AS INTEGER
  y AS INTEGER
END TYPE
CLASS MP
  PUBLIC DIM m(2, 2) AS Punto
  PUBLIC SUB NEW()
  END SUB
  FUNCTION Sum() AS INTEGER
    m(1, 2).x = 5
    m(1, 2).y = 7
    Sum = m(1, 2).x + m(1, 2).y
  END FUNCTION
END CLASS
SUB Main()
  DIM o AS MP
  o = NEW MP()
  PRINT "u="; o.Sum()
END SUB
