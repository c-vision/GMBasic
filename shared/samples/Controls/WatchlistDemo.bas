' WatchlistDemo.bas — demo d'applicazione del controllo (Controls/Watchlist.bas via IMPORT).
IMPORT CustomControls

SUB Main()
  DIM w AS Watchlist = NEW Watchlist()
  CONTROL ADD w AT 2, 2 SIZE 30, 12
  w.Append("AAPL", 100.0)
  w.Append("MSFT", 250.0)
  w.Append("NVDA", 500.0)
  w.ApplyTick("AAPL", 102.0)      ' +2.0%
  w.ApplyTick("MSFT", 245.0)      ' -2.0%
  PRINT "n="; w.Count()
  PRINT "AAPL idx="; w.IndexOf("AAPL"); " chg%="; w.ChangePct(0)
  PRINT "MSFT chg%="; w.ChangePct(1)
  DIM r AS INTEGER
  r = w.Hit(5, 1)
  PRINT "sel="; r; "/"; w.SelectedRow()
  w.RemoveRow(2)
  PRINT "n2="; w.Count()
  CONTROL REMOVE w
  PRINT "watchlist-ok"
END SUB
