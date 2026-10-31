' CandleChartDemo.bas — demo d'applicazione del controllo (Controls/CandleChart.bas via IMPORT).
IMPORT CustomControls

SUB Main()
  DIM ch AS CandleChart = NEW CandleChart()
  CONTROL ADD ch AT 2, 2 SIZE 78, 20
  ch.SetOrigin(2, 2, 78, 20)

  DIM i AS INTEGER
  DIM px AS DOUBLE
  px = 100.0
  FOR i = 0 TO 39
    DIM o1 AS DOUBLE
    DIM c1 AS DOUBLE
    DIM h1 AS DOUBLE
    DIM l1 AS DOUBLE
    o1 = px
    c1 = px + ((i * 37) MOD 11) - 5
    IF c1 > o1 THEN h1 = c1 + 4 ELSE h1 = o1 + 4
    IF c1 < o1 THEN l1 = c1 - 4 ELSE l1 = o1 - 4
    ch.AddCandle(o1, h1, l1, c1)
    px = c1
  NEXT i

  ch.Fit()                                   ' mostra tutte le candele
  ch.SetOverlay(3, 5)                        ' SMA(5) + EMA(5)
  ch.AddTrend(2.0, 96.0, 30.0, 108.0)        ' trendline in coordinate DATI
  ch.AddHLine(104.0)                         ' supporto/resistenza
  ch.AddVLine(20.0)                          ' marker temporale
  ch.AddMarker(35, 100.0, 0)                 ' segnale BUY
  ch.SetCrosshair(40, 10)                    ' crosshair (coordinate LOCALI)

  PRINT "candles="; ch.Count(); " view="; ch.ViewCount(); " drawings="; ch.DrawingCount()
  PRINT "cross="; ch.CrossIndex(); " markers="; ch.MarkerCount(); " overlay="; ch.OverlayReady()
  PRINT "OK-candlechart-uc"
END SUB
