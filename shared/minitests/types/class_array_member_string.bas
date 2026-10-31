' MiniTest: array-membro di CLASSE con elementi STRING — bare `arr(i)` + `SELF.arr(i)`,
' scrittura e lettura, concatenazione.
' EXPECT: cat=abc
CLASS C
  PUBLIC DIM arr(2) AS STRING
  PUBLIC SUB NEW()
    arr(0) = "a"
    SELF.arr(1) = "b"
    arr(2) = "c"
  END SUB
  FUNCTION Cat() AS STRING
    Cat = arr(0) + SELF.arr(1) + arr(2)
  END FUNCTION
END CLASS
SUB Main()
  DIM c AS C = NEW C()
  PRINT "cat="; c.Cat()
END SUB
