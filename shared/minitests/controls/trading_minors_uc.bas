' Minitest — controlli di dominio "minori" (§12.4) come UserControl, 4 backend.
' Ticker (flash up/down) · TreeGroup (espansione + totale gruppo) · EconCalendar (filtro impatto)
' · Toolbar (hit-test). EXPECT: trading-minors-ok
IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  ' --- Ticker ---
  DIM tk AS Ticker = NEW Ticker()
  tk.Init("AAPL", 100.0)
  tk.Quote(101.5)
  DIM tDir AS INTEGER
  tDir = tk.Direction()
  DIM tFlash AS INTEGER
  tFlash = tk.Flash()
  tk.Tick() : tk.Tick() : tk.Tick()
  DIM tFlash2 AS INTEGER
  tFlash2 = tk.Flash()
  tk.Quote(99.0)
  DIM tDir2 AS INTEGER
  tDir2 = tk.Direction()
  DIM tLast AS DOUBLE
  tLast = tk.Price()

  ' --- TreeGroup ---
  DIM tg AS TreeGroup = NEW TreeGroup()
  tg.AddGroup("Tech")
  tg.AddPosition(0, "AAPL", 10)
  tg.AddPosition(0, "MSFT", 20)
  tg.AddGroup("Energy")
  tg.AddPosition(3, "ENI", 5)
  DIM tgRows AS INTEGER
  DIM tgVis0 AS INTEGER
  tgRows = tg.Count()
  tgVis0 = tg.VisibleCount()
  tg.Toggle(0)
  DIM tgVis1 AS INTEGER
  tgVis1 = tg.VisibleCount()
  DIM tgQty AS INTEGER
  tgQty = tg.GroupQty(0)

  ' --- EconCalendar ---
  DIM ec AS EconCalendar = NEW EconCalendar()
  ec.AddEvent("14:30", "CPI", 2)
  ec.AddEvent("16:00", "Fiducia", 1)
  ec.AddEvent("20:00", "FOMC", 2)
  ec.SetMinImpact(2)
  DIM ecHigh AS INTEGER
  ecHigh = ec.MatchCount()
  ec.SetMinImpact(1)
  DIM ecAll AS INTEGER
  ecAll = ec.MatchCount()

  ' --- Toolbar ---
  DIM tb AS Toolbar = NEW Toolbar()
  tb.SetOrigin(0, 0, 40, 1)
  tb.Configure(0)
  tb.AddButton("Cut") : tb.AddButton("Copy") : tb.AddButton("Paste") : tb.AddButton("Undo")
  DIM tbHit AS INTEGER
  tbHit = tb.Hit(25, 0)
  DIM tbSel AS INTEGER
  DIM tbTxt AS STRING
  tbSel = tb.Selected()
  tbTxt = tb.SelectedText()

  IF tDir = 1 AND tFlash = 8 AND tFlash2 = 5 AND tDir2 = -1 AND ABS(tLast - 99.0) < 0.001 AND tgRows = 5 AND tgVis0 = 2 AND tgVis1 = 4 AND tgQty = 30 AND ecHigh = 2 AND ecAll = 3 AND tbHit = 2 AND tbSel = 2 AND tbTxt = "Paste" THEN
    PRINT "trading-minors-ok"
  ELSE
    PRINT "trading-minors-no " + STR(tDir) + "|" + STR(tFlash) + "|" + STR(tFlash2) + "|" + STR(tDir2) + "|" + STR(tgRows) + "|" + STR(tgVis0) + "|" + STR(tgVis1) + "|" + STR(tgQty) + "|" + STR(ecHigh) + "|" + STR(ecAll) + "|" + STR(tbHit) + "|" + STR(tbSel) + "|" + STR(tLast) + "|" + tbTxt
  END IF
END SUB
