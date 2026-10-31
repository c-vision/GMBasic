' FUNCTION/SUB LOCALI (closure-context): argomenti emessi UNA volta nel path OP_CALL_CTOR.
' BUG (PCode/VM-C): `Stampa("Mul(5)", Moltiplica(5))` stampava ` 5  ->  50` — il loop
' generico degli argomenti + il ramo OP_CALL_CTOR li spingevano DUE volte, sovrascrivendo
' il primo argomento col secondo. EXPECT: A: 50|B ->  7|C ->  50
SUB Main()
  DIM fattore = 10
  FUNCTION Moltiplica(x) AS DOUBLE
    FUNCTION = x * fattore
  END FUNCTION
  SUB Stampa(desc$, val)
    PRINT desc$; " -> "; val
  END SUB
  PRINT "A:"; Moltiplica(5)
  Stampa("B", 7)
  Stampa("C", Moltiplica(5))
END SUB
