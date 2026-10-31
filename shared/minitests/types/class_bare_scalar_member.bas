' MiniTest: campo SCALARE di CLASSE accessibile in forma NUDa dentro un metodo
' (`v = 5` / `v + w`) — deve riferirsi a `SELF.<campo>`, non a un locale.
' Copre il bug (open) Class-Bare-Scalar-Member-Trattato-Come-Locale.
' EXPECT: bare= 12|self= 12|ext= 12
CLASS C
  PUBLIC DIM v AS INTEGER
  PUBLIC DIM w AS INTEGER
  PUBLIC SUB NEW()
    w = 7
  END SUB
  SUB SetIt()
    v = 5
  END SUB
  FUNCTION GetBare() AS INTEGER
    GetBare = v + w
  END FUNCTION
  FUNCTION GetSelf() AS INTEGER
    GetSelf = SELF.v + SELF.w
  END FUNCTION
END CLASS
SUB Main()
  DIM c AS C = NEW C()
  c.SetIt()
  PRINT "bare="; c.GetBare()
  PRINT "self="; c.GetSelf()
  PRINT "ext="; c.v + c.w
END SUB
