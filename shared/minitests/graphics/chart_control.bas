' Minitest CONTROLS CHART — controllo a celle (motore autonomo gmb_chart), parità 4 backend.
' Il verbo CHART come CONTROLLO (non comando): `CHART x,y,w,h[,"v1;v2;..."]`.
' EXPECT: chartctl-ok
IMPORT CHART
SUB Main()
  ' controllo chart vuoto (demo sinusoide) e con serie esplicita
  CHART 2, 2, 40, 12
  CHART 2, 16, 40, 12, "10;20;15;30;25;40;35"
  ' forma x,y
  CHART 2, 30, 40, 12, "1,5;2,9;3,7;4,12;5,10"
  PRINT "chartctl-ok"
END SUB
