' MiniTest: bare call a un metodo/function della PROPRIA classe (es. `B()` dentro `A()`),
' risolto come `SELF.B()` su tutti i backend. Regressione storica: NPE di link
' (TypeParameters ImmutableArray default) + receiver SELF non passato su PCode.
' EXPECT: bare-method-ok
CLASS T
  DIM n AS INTEGER
  SUB A()
    n = 7
    IF F(1) = 2 THEN B()
  END SUB
  SUB B()
    IF n = 7 THEN PRINT "bare-method-ok" ELSE PRINT "bare-method-FAIL"
  END SUB
  FUNCTION F(x AS INTEGER) AS INTEGER
    RETURN x + 1
  END FUNCTION
END CLASS

SUB Main()
  DIM t AS T = NEW T()
  t.A()
END SUB
