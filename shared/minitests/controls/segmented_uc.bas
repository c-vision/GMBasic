' Minitest — Segmented / toggle-group UserControl (§12.4): items, selezione, hit-test
' orizzontale/verticale, move ciclico. Parità 4 backend.
' EXPECT: segmented-ok
IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM s AS Segmented = NEW Segmented()
  s.Configure(0)
  s.AddItem("1m") : s.AddItem("5m") : s.AddItem("15m") : s.AddItem("1h")
  s.SetOrigin(0, 0, 40, 3)
  s.SetSelected(2)
  DIM n AS INTEGER
  DIM sel2 AS INTEGER
  DIM txt AS STRING
  n = s.Count() : sel2 = s.Selected() : txt = s.SelectedText()
  DIM h1 AS INTEGER
  h1 = s.Hit(15, 1)
  DIM selA AS INTEGER
  selA = s.Selected()
  DIM h3 AS INTEGER
  h3 = s.Hit(35, 1)
  DIM selB AS INTEGER
  selB = s.Selected()
  DIM hn AS INTEGER
  hn = s.Hit(-1, 1)
  DIM hw AS INTEGER
  hw = s.Hit(40, 1)
  s.MoveSel(1)
  DIM mw AS INTEGER
  mw = s.Selected()
  s.MoveSel(-1)
  DIM mb AS INTEGER
  mb = s.Selected()
  s.Configure(1)
  s.SetOrigin(0, 0, 4, 40)
  DIM v3 AS INTEGER
  v3 = s.SegmentAt(1, 35)

  IF n = 4 AND sel2 = 2 AND txt = "15m" AND h1 = 1 AND selA = 1 AND h3 = 3 AND selB = 3 AND hn = -1 AND hw = -1 AND mw = 0 AND mb = 3 AND v3 = 3 THEN
    PRINT "segmented-ok"
  ELSE
    PRINT "segmented-no"
  END IF
END SUB
