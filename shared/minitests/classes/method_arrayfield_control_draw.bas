' Metodo di classe: campo-array letto in un FOR + CONTROL DRAW con argomenti espressione (i - 1, campo-array diretto, handle `g AS ANY`).
' Regressione IL: gli argomenti di CONTROL <sub> non venivano risolti/riscritti (g -> `ldarg.0`=this, mQty(i) -> array locale, conv.r8 su object).
' EXPECT: q 0 = 3|q 1 = 4|OK
IMPORT CONTROLS

CLASS Depth
  PRIVATE DIM mQty(8) AS DOUBLE
  PRIVATE DIM mRows AS INTEGER

  SUB SetLevel(i AS INTEGER, size AS DOUBLE)
    mQty(i) = size
    mRows = i + 1
  END SUB

  SUB Show(tag AS ANY)
    DIM q AS DOUBLE
    DIM i AS INTEGER
    FOR i = 0 TO mRows - 1
      q = mQty(i)
      PRINT "q"; i; "="; q
    NEXT i
  END SUB

  SUB Draw(g AS ANY)
    DIM i AS INTEGER
    FOR i = 1 TO mRows - 1
      CONTROL DRAW g, 4, i - 1, mQty(i), 2, 1
    NEXT i
  END SUB
END CLASS

SUB Main()
  DIM d AS Depth = NEW Depth()
  DIM h AS ANY
  d.SetLevel(0, 3)
  d.SetLevel(1, 4)
  d.Show(h)
  d.Draw(h)
  PRINT "OK"
END SUB
