' TradingMinorsDemo.bas — demo d'applicazione del controllo (Controls/TradingMinors.bas via IMPORT).
IMPORT CustomControls

SUB Main()
  DIM tk AS Ticker = NEW Ticker()
  CONTROL ADD tk AT 2, 2 SIZE 20, 1
  tk.SetOrigin(2, 2, 20, 1)
  tk.Init("AAPL", 100.0)
  tk.Quote(101.5)
  PRINT "ticker="; tk.Symbol(); " px="; tk.Price(); " dir="; tk.Direction(); " flash="; tk.Flash()

  DIM tg AS TreeGroup = NEW TreeGroup()
  CONTROL ADD tg AT 2, 4 SIZE 40, 8
  tg.SetOrigin(2, 4, 40, 8)
  tg.AddGroup("Tech")
  tg.AddPosition(0, "AAPL", 10)
  tg.AddPosition(0, "MSFT", 20)
  tg.AddGroup("Energy")
  tg.AddPosition(3, "ENI", 5)
  tg.Toggle(0)
  PRINT "tree rows="; tg.Count(); " visible="; tg.VisibleCount(); " techQty="; tg.GroupQty(0)

  DIM ec AS EconCalendar = NEW EconCalendar()
  CONTROL ADD ec AT 2, 13 SIZE 40, 8
  ec.SetOrigin(2, 13, 40, 8)
  ec.AddEvent("14:30", "CPI USA", 2)
  ec.AddEvent("16:00", "Fiducia UE", 1)
  ec.AddEvent("20:00", "FOMC", 2)
  ec.SetMinImpact(2)
  PRINT "econ events="; ec.Count(); " high="; ec.MatchCount()

  DIM tb AS Toolbar = NEW Toolbar()
  CONTROL ADD tb AT 2, 22 SIZE 40, 1
  tb.SetOrigin(2, 22, 40, 1)
  tb.Configure(0)
  tb.AddButton("Cut") : tb.AddButton("Copy") : tb.AddButton("Paste") : tb.AddButton("Undo")
  tb.Hit(25, 0)
  PRINT "toolbar sel="; tb.Selected(); " label="; tb.SelectedText()

  PRINT "trading-minors-uc-ok"
END SUB
