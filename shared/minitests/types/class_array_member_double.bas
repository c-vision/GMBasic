' MiniTest: array-membro di CLASSE con elementi DOUBLE — bare `arr(i)` + `SELF.arr(i)`,
' scrittura e lettura, somma numerica.
' EXPECT: sum= 7
CLASS C
  PUBLIC DIM arr(2) AS DOUBLE
  PUBLIC SUB NEW()
    arr(0) = 1.5
    SELF.arr(1) = 2.5
    arr(2) = 3.0
  END SUB
  FUNCTION Total() AS DOUBLE
    Total = arr(0) + SELF.arr(1) + arr(2)
  END FUNCTION
END CLASS
SUB Main()
  DIM c AS C = NEW C()
  PRINT "sum="; c.Total()
END SUB
