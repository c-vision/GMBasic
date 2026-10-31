' Demo: chiamate bare ai metodi della PROPRIA classe (senza `SELF.`).
' Dentro un metodo, `ContaMovimento()`, `Bilancio()` ecc. risolvono implicitamente su SELF.
CLASS Conto
  DIM saldo AS DOUBLE
  DIM movimenti AS INTEGER

  SUB Accredita(importo AS DOUBLE)
    saldo = saldo + importo
    ContaMovimento()
  END SUB

  SUB Addebita(importo AS DOUBLE)
    saldo = saldo - importo
    ContaMovimento()
  END SUB

  SUB ContaMovimento()
    movimenti = movimenti + 1
  END SUB

  FUNCTION Bilancio() AS DOUBLE
    RETURN saldo
  END FUNCTION

  SUB Stampa()
    PRINT "movimenti="; movimenti; " saldo="; Bilancio()
  END SUB
END CLASS

SUB Main()
  DIM c AS Conto = NEW Conto()
  c.Accredita(100)
  c.Addebita(30)
  c.Accredita(5)
  c.Stampa()
END SUB
