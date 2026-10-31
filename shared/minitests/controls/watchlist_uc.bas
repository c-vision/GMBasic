' Minitest Watchlist UserControl (§12.4) — tabella simboli/prezzi + tick + hit-test + remove, 4 backend.
' EXPECT: watchlist-ok
IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM w AS Watchlist = NEW Watchlist()
  CONTROL ADD w AT 2, 2 SIZE 30, 12
  w.Append("AAA", 10.0)
  w.Append("BBB", 20.0)
  w.Append("CCC", 30.0)
  w.ApplyTick("AAA", 11.0)        ' +10%
  w.ApplyTick("ZZZ", 5.0)         ' aggiunge una nuova riga
  DIM idx AS INTEGER
  idx = w.IndexOf("BBB")
  DIM chgA AS DOUBLE
  chgA = w.ChangePct(0)           ' AAA = +10%
  DIM r AS INTEGER
  r = w.Hit(3, 1)
  DIM outr AS INTEGER
  outr = w.Hit(3, 99)             ' fuori
  DIM nBefore AS INTEGER
  nBefore = w.Count()
  w.RemoveRow(0)
  CONTROL REMOVE w
  IF nBefore = 4 AND idx = 1 AND ABS(chgA - 10.0) < 0.001 AND r = 1 AND outr = -1 AND w.Count() = 3 AND w.SymbolOf(0) = "BBB" AND ABS(w.PriceOf(0) - 20.0) < 0.001 THEN
    PRINT "watchlist-ok"
  ELSE
    PRINT "watchlist-no"
  END IF
END SUB
