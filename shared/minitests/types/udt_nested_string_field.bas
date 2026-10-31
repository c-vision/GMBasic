' MiniTest: campo UDT ANNIDATO via campo di CLASSE — `r.f`/`SELF.r.f` con `f AS STRING * 8`
' (lunghezza fissa: LEN 8) e campo numerico annidato `r.n` (bare e SELF).
' Bug: C99Native emetteva una variabile nuda (valore 0) non compilava, e
' PCode/VM-C perdevano il padding della lunghezza fissa.
' EXPECT: cat=x|len= 8|n= 7
TYPE R
  f AS STRING * 8
  n AS INTEGER
END TYPE
CLASS C
  PUBLIC DIM r AS R
  PUBLIC SUB NEW()
    r.f = "x"
    SELF.r.n = 7
  END SUB
  FUNCTION Cat() AS STRING
    Cat = SELF.r.f
  END FUNCTION
  FUNCTION GetN() AS INTEGER
    GetN = r.n
  END FUNCTION
END CLASS
SUB Main()
  DIM c AS C = NEW C()
  PRINT "cat="; c.Cat()
  PRINT "len="; LEN(c.Cat())
  PRINT "n="; c.GetN()
END SUB
