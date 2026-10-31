' CandleChart.bas — chart a candele come UserControl (stile VB6), scritto in BASIC.
' Sostituisce l'idea di CANDLESTICK built-in: ora e' un controllo utente (namespace CONTROLS).
IMPORT CONTROLS

CLASS CandleChart
  PRIVATE DIM mOpen(64) AS DOUBLE
  PRIVATE DIM mHigh(64) AS DOUBLE
  PRIVATE DIM mLow(64) AS DOUBLE
  PRIVATE DIM mClose(64) AS DOUBLE
  PRIVATE DIM mN AS INTEGER

  SUB AddCandle(po AS DOUBLE, ph AS DOUBLE, pl AS DOUBLE, pc AS DOUBLE)
    IF mN < 64 THEN
      mOpen(mN) = po
      mHigh(mN) = ph
      mLow(mN) = pl
      mClose(mN) = pc
      mN = mN + 1
    END IF
  END SUB

  SUB Paint(g AS ANY)
    DIM i AS INTEGER
    DIM top AS DOUBLE
    DIM bot AS DOUBLE
    DIM bodyH AS DOUBLE
    CONTROL DRAW g, 0                       ' clear (sfondo)
    IF mN <= 0 THEN EXIT SUB

    ' scala verticale: min/max
    DIM mn AS DOUBLE
    DIM mx AS DOUBLE
    mn = mLow(0)
    mx = mHigh(0)
    FOR i = 1 TO mN - 1
      IF mLow(i) < mn THEN mn = mLow(i)
      IF mHigh(i) > mx THEN mx = mHigh(i)
    NEXT i
    IF mx <= mn THEN mx = mn + 1

    DIM scaleY AS DOUBLE
    scaleY = 1.0 / (mx - mn)

    FOR i = 0 TO mN - 1
      ' wick: dalla massima alla minima (verde/rosso in bodyH a o vs c)
      IF mClose(i) >= mOpen(i) THEN
        CONTROL DRAW g, 1, 65280            ' verde
      ELSE
        CONTROL DRAW g, 1, 16711680         ' rosso
      END IF
      top = (mx - mHigh(i)) * scaleY
      bot = (mx - mLow(i)) * scaleY
      CONTROL DRAW g, 2, i, top, i, bot     ' wick (linea verticale)

      ' body: rettangolo pieno o-c
      IF mOpen(i) > mClose(i) THEN
        top = (mx - mOpen(i)) * scaleY
        bot = (mx - mClose(i)) * scaleY
      ELSE
        top = (mx - mClose(i)) * scaleY
        bot = (mx - mOpen(i)) * scaleY
      END IF
      bodyH = bot - top
      IF bodyH < 0 THEN bodyH = 0
      CONTROL DRAW g, 4, i - 1, top, 2, bodyH   ' corpo
    NEXT i
  END SUB
END CLASS

SUB Main()
  DIM ch AS CandleChart = NEW CandleChart()
  DIM i AS INTEGER
  DIM px AS DOUBLE
  px = 100
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

  CONTROL ADD ch AT 2, 2 SIZE 78, 20      ' contenitore = window corrente
  PRINT "OK-candlechart-uc"
END SUB
