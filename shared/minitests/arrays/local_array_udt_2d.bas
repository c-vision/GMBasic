' MiniTest: array UDT LOCALE 2-D (DIM m(2,2) AS Punto) —
' campo su elemento 2-D (m(1,2).x / .y).
' EXPECT: u= 12
TYPE Punto
  x AS INTEGER
  y AS INTEGER
END TYPE
SUB Main()
  DIM m(2, 2) AS Punto
  m(1, 2).x = 5
  m(1, 2).y = 7
  PRINT "u="; m(1, 2).x + m(1, 2).y
END SUB
