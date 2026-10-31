' ============================================================================
' CandlestickDemo — sample del controllo CONTROLS CANDLESTICK (candele OHLC).
' ============================================================================
' Statement (standalone, ramo pixel):
'     CANDLESTICK row, col, width, height [, "o,h,l,c;o,h,l,c;..."]
'   - row/col/width/height: area del widget.
'   - 5° argomento opzionale: candele separate da ';', ogni candela "o,h,l,c"
'     (open, high, low, close). Se assente → un dataset demo (12 candele).
'   - Resa: a CELLE in TEXT (blocchi U+2588/U+2502, verde=rialzo / rosso=ribasso),
'     a PIXEL in GRAPHICS. Navigazione con Left/Right/Home/End (cursore).
'
' Headless: il widget è MODALE; il sample inietta SENDKEY CHR(27) per chiuderlo
' sui backend C (canary/C99Native).
'
' Esecuzione:  tools/gmb_c99native_run.sh samples/Community/Controls/CandlestickDemo.bas
' ============================================================================
IMPORT CONTROLS

SUB Main()
  PRINT "=== CandlestickDemo ==="

  ' Dataset demo.
  CANDLESTICK 1, 1, 50, 12
  SENDKEY CHR(27)

  ' Candele esplicite (trend rialzista).
  CANDLESTICK 14, 1, 50, 12, "10,12,9,11;11,13,10,12;12,13.5,11,13.2;13.2,15,12.8,14.6;14.6,16,14,15.5"
  SENDKEY CHR(27)

  PRINT "=== fine ==="
END SUB
