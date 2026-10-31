' ============================================================================
' ChartControlDemo — CONTROLS CHART: il grafico come CONTROLLO a celle.
'
'   CHART x, y, w, h [, "serie"]
'
'   "serie" = "v1;v2;..."              (x = indice 1,2,3,...)
'          oppure "x1,y1;x2,y2;..."     (coppie)
'          vuoto → sinusoide demo.
'
' Il controllo è reso a CELLE in TEXT e a pixel in GRAPHICS dal motore autonomo
' gmb_chart (stesso core dei comandi CHART). Ridimensiona col controllo.
'
' NB: è il verbo CHART nella forma CONTROLLO (non comando). Disambiguazione:
'   - CHART SET/GET/GETS/PLOT/FINANCIAL/DRAW/SHOW/SAVE/CLEAR/CLOSE  → COMANDO
'   - CHART x,y,w,h[,"csv"]                                        → CONTROLLO
'
' Parità 4 backend: MiniTest graphics/chart_control.bas.
' ============================================================================
IMPORT CHART

SUB Main()
  PRINT "=== ChartControlDemo ==="

  ' 1) Controllo con serie di valori (x = indice).
  CHART 2, 2, 60, 14, "10;20;15;30;25;40;35;50;45;60"

  ' 2) Controllo con coppie x,y.
  CHART 2, 18, 60, 14, "1,5;2,9;3,7;4,12;5,10;6,15"

  ' 3) Controllo con serie demo (csv vuoto).
  CHART 2, 34, 60, 14

  PRINT "chart-control-demo-ok"
END SUB
