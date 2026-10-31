' Minitest — CandleChart interattivo UserControl (§12.4): vista/zoom/pan, crosshair,
' drawing tools (trend/hline/vline + pick/move/delete), overlay SMA, marker. Parità 4 backend.
' EXPECT: candlechart-int-ok
IMPORT CONTROLS
IMPORT CustomControls

SUB Main()
  DIM ch AS CandleChart = NEW CandleChart()
  DIM i AS INTEGER
  FOR i = 0 TO 9
    ch.AddCandle(100.0 + i, 102.0 + i, 98.0 + i, 100.0 + i)
  NEXT i
  ch.SetOrigin(0, 0, 40, 11)
  ch.Fit()
  DIM vf1 AS INTEGER
  DIM vc1 AS INTEGER
  vf1 = ch.ViewFirst() : vc1 = ch.ViewCount()
  ch.SetView(2, 5)
  DIM vf2 AS INTEGER
  vf2 = ch.ViewFirst()
  ch.PanBy(2)
  DIM vf3 AS INTEGER
  vf3 = ch.ViewFirst()
  ch.SetView(0, 10)
  ch.ZoomAt(0.5, 20)
  DIM vf4 AS INTEGER
  DIM vc4 AS INTEGER
  vf4 = ch.ViewFirst() : vc4 = ch.ViewCount()
  ch.SetYRange(100.0, 200.0)
  ch.SetCrosshair(20, 5)
  DIM ci AS INTEGER
  DIM cp AS DOUBLE
  DIM cc AS DOUBLE
  ci = ch.CrossIndex() : cp = ch.CrossPrice() : cc = ch.CrossClose()
  ch.ClearCrosshair()
  DIM clr AS INTEGER
  clr = ch.CrossIndex()
  ch.AddTrend(0.0, 100.0, 5.0, 110.0)
  ch.AddHLine(105.0)
  ch.AddVLine(2.0)
  DIM dc AS INTEGER
  dc = ch.DrawingCount()
  DIM sel AS INTEGER
  sel = ch.PickAt(10, 9)
  ch.MoveSelected(0, 0)
  DIM hy2 AS DOUBLE
  hy2 = ch.DrawingY2(1)
  ch.DeleteSelected()
  DIM dc2 AS INTEGER
  dc2 = ch.DrawingCount()
  ch.SetOverlay(1, 3)
  DIM ov AS INTEGER
  ov = ch.OverlayReady()
  DIM sm AS DOUBLE
  sm = ch.SmaValue(2)
  ch.AddMarker(5, 100.0, 0)
  ch.AddMarker(6, 101.0, 1)
  DIM mk AS INTEGER
  mk = ch.MarkerCount()

  IF vf1 = 0 AND vc1 = 10 AND vf2 = 2 AND vf3 = 4 AND vf4 = 3 AND vc4 = 5 AND ci = 6 AND ABS(cp - 150.0) < 0.001 AND ABS(cc - 106.0) < 0.001 AND clr = -1 AND dc = 3 AND sel = 1 AND ABS(hy2 - 200.0) < 0.001 AND dc2 = 2 AND ov = 1 AND ABS(sm - 101.0) < 0.001 AND mk = 2 THEN
    PRINT "candlechart-int-ok"
  ELSE
    PRINT "candlechart-int-no"
  END IF
END SUB
