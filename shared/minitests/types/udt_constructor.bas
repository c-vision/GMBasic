' TYPE con SUB NEW: il costruttore è invocato da NEW T(...) e popola i campi
' (emitter C# non emetteva/chiamava i costruttori dei TYPE → campi a 0).
' Campi NUMERICI per isolare il costruttore (il padding STRING*N è coperto da
' minitests/arrays/udt_fixed_string.bas).
' EXPECT: x= 7|y= 9|somma= 16
TYPE T
  X AS LONG
  Y AS LONG
  SUB NEW(a AS LONG, b AS LONG)
    SELF.X = a
    SELF.Y = b
  END SUB
  FUNCTION Somma() AS LONG
    FUNCTION = SELF.X + SELF.Y
  END FUNCTION
END TYPE
SUB Main()
  DIM t AS T = NEW T(7, 9)
  PRINT "x="; t.X
  PRINT "y="; t.Y
  PRINT "somma="; t.Somma()
END SUB
